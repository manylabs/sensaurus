#include <ProcessingQueue.h>
#include <Arduino.h>
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"


#define TAG "sensaurus"

const unsigned int ProcessingQueue::MAX_SIZE = 10;

ProcessingQueue::ProcessingQueue(): queue(ArduinoQueue<QueueItem>(MAX_SIZE)) {
}


bool ProcessingQueue::try_push(const QueueItem& item) {
  if (count() >= MAX_SIZE) {
    return false;
  }
  //ESP_LOGI(TAG, "ProcessingQueue::try_push");
  queue.push(item);
  return true;
}

QueueItem ProcessingQueue::try_pop() {
  return queue.pop();
}
