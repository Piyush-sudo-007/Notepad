import os
import torch
from transformers import GPT2LMHeadModel, GPT2Tokenizer, Trainer, TrainingArguments, DataCollatorForLanguageModeling
from datasets import load_dataset, interleave_datasets

os.environ["HF_HUB_DISABLE_SYMLINKS_WARNING"] = "1"

def main():
    model_name = "gpt2"
    tokenizer = GPT2Tokenizer.from_pretrained(model_name)
    model = GPT2LMHeadModel.from_pretrained(model_name)
    tokenizer.pad_token = tokenizer.eos_token

    
    code_dataset = load_dataset("m-a-p/CodeFeedback-Filtered-Instruction", split="train", streaming=True)
    
    english_text = load_dataset("vicgalle/alpaca-gpt4", split="train", streaming=True)

    def clean_code_format(example):
        return {"text": f"{example.get('query', '')}\n{example.get('answer', '')}"}

    def clean_text_format(example):
        instruction = example.get('instruction', '')
        input_data = example.get('input', '')
        output = example.get('output', '')
        
        if input_data:
            return {"text": f"{instruction}\nContext: {input_data}\n{output}"}
        return {"text": f"{instruction}\n{output}"}

    code_stream = code_dataset.map(clean_code_format)
    text_stream = english_text.map(clean_text_format)

    mixed_dataset = interleave_datasets(
        [code_stream, text_stream],
        probabilities=[0.5, 0.5],
        seed=42
    )

    def tokenize_function(examples):
        return tokenizer(examples["text"], truncation=True, max_length=128)

    dataset_subset = mixed_dataset.take(20000)
    
    def batch_iterator():
        for example in dataset_subset:
            processed_tokens = tokenize_function(example)
            if processed_tokens["input_ids"]:
                yield processed_tokens

    training_args = TrainingArguments(
        output_dir="./smartpad_english_checkpoint",
        max_steps=2000,                      # Balanced runtime for solid training depth
        per_device_train_batch_size=4,       # Low memory footprint safe for general laptops
        logging_steps=100,
        save_steps=1000,
        learning_rate=4e-5,                  # Preserves fundamental base structural weights
        weight_decay=0.01,
        fp16=torch.cuda.is_available()       # Accelerates via local Nvidia GPU if available
    )

    class IterableDatasetWrapper(torch.utils.data.IterableDataset):
        def __init__(self, iterator_func, length):
            self.iterator_func = iterator_func
            self.length = length
        def __iter__(self):
            return self.iterator_func()
        def __len__(self):
            return self.length

    data_collator = DataCollatorForLanguageModeling(tokenizer=tokenizer, mlm=False)
    
    trainer = Trainer(
        model=model,
        args=training_args,
        data_collator=data_collator,
        train_dataset=IterableDatasetWrapper(batch_iterator, length=20000)
    )

    print("--- 5. Running English Fine-Tuning Optimization ---")
    trainer.train()

    output_model_dir = "./fine_tuned_english_smartpad"
    model.save_pretrained(output_model_dir)
    tokenizer.save_pretrained(output_model_dir)
    
    os.system(f"python -m optimum.exporters.onnx --model {output_model_dir} --task causal-lm assets_onnx/")

if __name__ == "__main__":
    main()