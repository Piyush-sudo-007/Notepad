#include "predictionworker.h"
#include "tiktoken.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QCoreApplication>
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
  qDebug() << "Loading model from:" << modelPath;
  try
  {
    Ort::SessionOptions sessionOptions;
    sessionOptions.SetIntraOpNumThreads(2);
    sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

    std::wstring wideModelPath = modelPath.toStdWString();
    session = std::make_unique<Ort::Session>(env, wideModelPath.c_str(), sessionOptions);
    qDebug() << "Model loaded successfully.";
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
  qDebug() << "Processing prediction for context:" << contextText;
  if (!session || contextText.isEmpty())
  {
    qDebug() << "Model not loaded or empty context.";
    return;
  }

  try
  {
    QString absoluteTomlPath = QCoreApplication::applicationDirPath() + "/assets/tiktoken.toml";
    qDebug() << "Loading tokenizer config from:" << absoluteTomlPath;

    sw::tokenizer::TiktokenFactory factory(absoluteTomlPath.toStdString());
    sw::tokenizer::Tiktoken tokenizerInstance = factory.create("p50k_base");

    std::string contextStr = contextText.toStdString();
    std::vector<uint64_t> tokensInt = tokenizerInstance.encode(contextStr);

    qDebug() << "Encoded tokens count:" << tokensInt.size();

    if (tokensInt.empty())
    {
      qDebug() << "Warning: No tokens generated from the input context.";
      return;
    }

    // Limit window context lengths matching standard GPT-2 generation sizes
    if (tokensInt.size() > 32)
    {
      tokensInt.erase(tokensInt.begin(), tokensInt.end() - 32);
    }

    std::vector<int64_t> inputTokens;
    for (uint64_t t : tokensInt)
    {
      inputTokens.push_back(static_cast<int64_t>(t));
    }

    // 3D tensor layout mapping shape: [Batch (1), Sequence Length, 1]
    std::vector<int64_t> inputDims = {1, static_cast<int64_t>(inputTokens.size()), 1};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, inputTokens.data(), inputTokens.size(), inputDims.data(), inputDims.size());

    const char *inputNames[] = {"input1"};
    const char *outputNames[] = {"output1"};

    auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
    qDebug() << "ONNX session executed successfully.";

    auto typeInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
    std::vector<int64_t> shape = typeInfo.GetShape();
    qDebug() << "Output tensor shape:" << shape;

    float *rawLogits = outputTensors[0].GetTensorMutableData<float>();
    const size_t vocabSize = 50257;

    // ==================== FIXED POINTER OFFSETS ====================
    // Dimensions: [Batch (1), SeqLen, Dim (1), VocabSize (50257)]
    // To read the next-word distribution for the *last* typed word,
    // we skip past exactly (SeqLen - 1) token blocks.
    size_t lastTokenIndex = inputTokens.size() - 1;
    size_t targetOffset = lastTokenIndex * 1 * vocabSize;

    std::vector<float> finalLogits(
        rawLogits + targetOffset,
        rawLogits + targetOffset + vocabSize);
    // ===============================================================

    // Incorporate custom vocabulary logs from user profile weights database safely across threads
    {
      // Open database instance using a safe thread-local alias hook
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
    }

    std::vector<std::pair<float, int>> indexedLogits;
    for (size_t i = 0; i < finalLogits.size(); ++i)
    {
      indexedLogits.push_back({finalLogits[i], static_cast<int>(i)});
    }

    // Retrieve the top 3 recommended token structures using descending probability score order
    std::partial_sort(indexedLogits.begin(), indexedLogits.begin() + 3, indexedLogits.end(),
                      [](const std::pair<float, int> &a, const std::pair<float, int> &b)
                      {
                        return a.first > b.first;
                      });

    QStringList recommendations;
    for (int i = 0; i < 3; ++i)
    {
      std::vector<uint64_t> singleToken = {static_cast<uint64_t>(indexedLogits[i].second)};
      std::string decodedWord = tokenizerInstance.decode(singleToken);
      qDebug() << "Decoded token ID" << indexedLogits[i].second << "to word:" << QString::fromStdString(decodedWord);

      QString wordResult = QString::fromStdString(decodedWord);
      if (!wordResult.isEmpty())
      {
        recommendations << wordResult;
      }
    }

    qDebug() << "Predictions generated:" << recommendations;

    emit predictionsReady(recommendations);
  }
  catch (const std::exception &e)
  {
    qDebug() << "Prediction failed:" << e.what();
  }
}