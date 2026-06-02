#ifndef PREDICTIONWORKER_H
#define PREDICTIONWORKER_H

#include <QObject>
#include <QStringList>
#include <memory>
#include <vector>
#include <onnxruntime_cxx_api.h>

class PredictionWorker : public QObject
{
  Q_OBJECT
public:
  explicit PredictionWorker(QObject *parent = nullptr);
  ~PredictionWorker() override;

public slots:
  void loadModel(const QString &modelPath);
  void processPrediction(const QString &contextText, const QString &mode);

signals:
  void predictionReady(const QStringList &suggestions);
  void modelLoadedStatus(bool success);

private:
  Ort::Env env;
  std::unique_ptr<Ort::Session> session;

  size_t vocabSize = 50257;
};

#endif