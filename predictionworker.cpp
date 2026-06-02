#include "predictionworker.h"
#include "tiktoken.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <cmath>
#include <algorithm>

PredictionWorker::PredictionWorker(QObject *parent)
    : QObject(parent),
      env(ORT_LOGGING_LEVEL_WARNING, "SmartPadEngine")
{
}

PredictionWorker::~PredictionWorker() = default;

void PredictionWorker::loadModel(const QString &modelPath)
{
  try
  {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(2);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    std::wstring wideModelPath = modelPath.toStdWString();
    session = std::make_unique<Ort::Session>(env, wideModelPath.c_str(), sessionOptions);
    emit modelLoadedStatus(true);
  }
  catch (const std::exception &e)
  {
    qDebug() << "Failed to load model:" << e.what();
    emit modelLoadedStatus(false);
  }
}

void PredictionWorker::processPrediction(const QString &contextText, const QString &mode)
{
  if (!session || contextText.isEmpty())
  {
    qDebug() << "Model not loaded.";
    return;
  }

  try
  {
    // auto tiktokens = third_party::sw::tokenizer::Tiktoken::tiktoken_init("assets/tiktoken.toml");

    sw::tokenizer::TiktokenFactory factory("assets/tiktoken.toml");

    sw::tokenizer::Tiktoken tiktokens = factory.create("gpt2");

    std::string contextStr = contextText.toStdString();
    // std::vector<int> tokensInt = tiktokens.encode(contextStr);
    std::vector<uint64_t> tokensInt = tiktokens.encode(contextStr);

    if (tokensInt.empty())
      return;

    if (tokensInt.size() > 32)
    {
      tokensInt.erase(tokensInt.begin(), tokensInt.end() - 32);
    }

    std::vector<int64_t> inputTokens;
    for (int t : tokensInt)
      inputTokens.push_back(static_cast<int64_t>(t));

    std::vector<int64_t> inputDims = {1, static_cast<int64_t>(inputTokens.size())};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, inputTokens.data(), inputTokens.size(), inputDims.data(), inputDims.size());

    const char *inputNames[] = {"input_ids"};
    const char *outputNames[] = {"logits"};

    auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
    float *rawLogits = outputTensors.front().GetTensorMutableData<float>();

    std::vector<float> finalLogits(
        rawLogits + (inputTokens.size() - 1) * vocabSize,
        rawLogits + inputTokens.size() * vocabSize);

    QSqlDatabase db = QSqlDatabase::database();
    if (db.isOpen())
    {
      QSqlQuery query;
      query.prepare("SELECT TokenID, BiasCount FROM UserVocabulary WHERE ProfileMode = :mode");
      query.bindValue(":mode", mode);
      if (query.exec())
      {
        while (query.next())
        {
          int tokenId = query.value(0).toInt();
          int count = query.value(1).toInt();
          if (tokenId >= 0 && static_cast<size_t>(tokenId) < finalLogits.size())
          {
            finalLogits[tokenId] += (static_cast<float>(count) * 0.45f);
          }
        }
      }
    }

    std::vector<std::pair<float, int>> indexedLogits;
    for (size_t i = 0; i < finalLogits.size(); ++i)
    {
      indexedLogits.push_back({finalLogits[i], static_cast<int>(i)});
    }
    std::partial_sort(indexedLogits.begin(), indexedLogits.begin() + 3, indexedLogits.end(), [](const std::pair<float, int> &a, const std::pair<float, int> &b)
                      { return a.first > b.first; });

    QStringList recommendations;
    for (int i = 0; i < 3; ++i)
    {
      std::vector<uint64_t> singleToken = {static_cast<uint64_t>(indexedLogits[i].second)};
      std::string decodedWord = tiktokens.decode(singleToken);

      QString wordResult = QString::fromStdString(decodedWord);
      if (!wordResult.trimmed().isEmpty())
      {
        recommendations << wordResult;
      }
    }
    emit predictionReady(recommendations);
  }
  catch (const std::exception &e)
  {
    qDebug() << "Prediction failed:" << e.what();
  }
}