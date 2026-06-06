#include "predictionworker.h"
#include "tiktoken.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>
#include <QCoreApplication>
#include <QThread>
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
    emit modelLoadedStatus(false);
  }
}

void PredictionWorker::processPrediction(const QString &contextText, const QString &mode)
{
  if (!session || contextText.isEmpty())
  {
    return;
  }

  try
  {
    QString absoluteTomlPath = QCoreApplication::applicationDirPath() + "/assets/tiktoken.toml";

    sw::tokenizer::TiktokenFactory factory(absoluteTomlPath.toStdString());
    sw::tokenizer::Tiktoken tokenizerInstance = factory.create("p50k_base");

    QString trimmedContext = contextText.trimmed();
    trimmedContext = trimmedContext.replace(QChar(0x00A0), " ");
    if (trimmedContext.isEmpty())
    {
      return;
    }

    std::string contextStr = trimmedContext.toStdString();
    std::vector<uint64_t> tokensInt = tokenizerInstance.encode(contextStr);

    if (tokensInt.empty())
    {
      return;
    }

    // Limit window context lengths matching standard GPT-2 generation sizes
    if (tokensInt.size() > 32)
    {
      tokensInt.erase(tokensInt.begin(), tokensInt.end() - 32);
    }

    const size_t vocabSize = 50257;
    std::vector<int64_t> inputTokens;

    for (uint64_t t : tokensInt)
    {
      if (t >= vocabSize)
      {
        inputTokens.push_back(static_cast<int64_t>(vocabSize - 1));
      }
      else
      {
        inputTokens.push_back(static_cast<int64_t>(t));
      }
    }

    std::vector<int64_t> inputDims = {1, static_cast<int64_t>(inputTokens.size())};
    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU);

    Ort::Value inputTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, inputTokens.data(), inputTokens.size(), inputDims.data(), inputDims.size());

    std::vector<int64_t> positionTokens;
    std::vector<int64_t> attentionMaskTokens;

    for (size_t i = 0; i < inputTokens.size(); ++i)
    {
      positionTokens.push_back(static_cast<int64_t>(i));
      attentionMaskTokens.push_back(1);
    }

    Ort::Value positionTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, positionTokens.data(), positionTokens.size(), inputDims.data(), inputDims.size());

    Ort::Value maskTensor = Ort::Value::CreateTensor<int64_t>(
        memoryInfo, attentionMaskTokens.data(), attentionMaskTokens.size(), inputDims.data(), inputDims.size());

    const char *inputNames[] = {"input_ids", "attention_mask", "position_ids"};
    const char *outputNames[] = {"logits"};

    Ort::Value inputTensors[] = {std::move(inputTensor), std::move(maskTensor), std::move(positionTensor)};

    auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, inputTensors, 3, outputNames, 1);

    auto typeInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
    std::vector<int64_t> shape = typeInfo.GetShape();

    float *rawLogits = outputTensors[0].GetTensorMutableData<float>();

    // ==================== FIXED POINTER OFFSETS ====================

    int64_t batchStride = shape[1] * shape[2];
    int64_t seqStride = shape[2];

    int64_t lastTokenIdx = static_cast<int64_t>(inputTokens.size()) - 1;
    int64_t targetOffset = (0 * batchStride) + (lastTokenIdx * seqStride);

    std::vector<float> finalLogits(
        rawLogits + targetOffset,
        rawLogits + targetOffset + vocabSize);
    // ===============================================================

    float temperature = 0.65f;
    if (temperature > 0.0f && std::abs(temperature - 1.0f) > 0.001f)
    {
      for (size_t i = 0; i < finalLogits.size(); ++i)
      {
        finalLogits[i] /= temperature;
      }
    }

    QString threadDbConnectionName = QString("PredictionWorker_DB_%1").arg(quintptr(QThread::currentThreadId()));
    QSqlDatabase db;

    if (QSqlDatabase::contains(threadDbConnectionName))
    {
      db = QSqlDatabase::database(threadDbConnectionName);
    }
    else
    {
      db = QSqlDatabase::addDatabase("QSQLITE", threadDbConnectionName);
      db.setDatabaseName(QCoreApplication::applicationDirPath() + "/user_smartprofile.db");
    }

    if (!db.isOpen() && !db.open())
    {
      return;
    }
    if (db.isOpen() || db.open())
    {
      QSqlQuery query(db);
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
            float logBias = 1.35f * std::log1p(static_cast<float>(count));
            finalLogits[tokenId] += logBias;
          }
        }
      }
    }

    std::vector<std::pair<float, int>> indexedLogits;
    for (size_t i = 0; i < finalLogits.size(); ++i)
    {
      indexedLogits.push_back({finalLogits[i], static_cast<int>(i)});
    }

    std::partial_sort(indexedLogits.begin(), indexedLogits.begin() + 50, indexedLogits.end(),
                      [](const std::pair<float, int> &a, const std::pair<float, int> &b)
                      {
                        return a.first > b.first;
                      });

    QStringList contextWords = contextText.split(' ', Qt::SkipEmptyParts);
    QString currentPartialWord = "";

    if (!contextText.endsWith(" ") && !contextWords.isEmpty())
    {
      currentPartialWord = contextWords.last().toLower();
    }

    QStringList recommendations;
    for (int i = 0; i < 500 && recommendations.size() < 5; ++i)
    {
      std::vector<uint64_t> singleToken = {static_cast<uint64_t>(indexedLogits[i].second)};
      std::string decodedWord = tokenizerInstance.decode(singleToken);

      QString wordResult = QString::fromStdString(decodedWord);
      if (wordResult.contains('\n') || wordResult.contains('\r') || wordResult.isEmpty())
      {
        continue;
      }

      QString cleanWord = wordResult.trimmed();
      if (cleanWord.isEmpty() || cleanWord == "." || cleanWord == "," || cleanWord == "!" || cleanWord == "?" || cleanWord == "-" || cleanWord == "_")
      {
        continue;
      }

      if (!currentPartialWord.isEmpty())
      {
        if (!cleanWord.toLower().startsWith(currentPartialWord) || cleanWord.toLower() == currentPartialWord)
        {
          continue;
        }
      }

      if (!recommendations.contains(cleanWord))
      {
        recommendations << cleanWord;
      }
    }
    emit predictionsReady(recommendations);
  }
  catch (const std::exception &e)
  {
    qDebug() << "Prediction failed:" << e.what();
  }
}