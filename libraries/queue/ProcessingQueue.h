#ifndef _PROCESSINGQUEUE_H_
#define _PROCESSINGQUEUE_H_
#include "ArduinoQueue.h"

// Type of request to be processed
enum RequestType {
    REQUEST_NONE = 0,
    REQUEST_MQTT_PUBLISH_STATUS = 1,
    REQUEST_ACTUATOR_SET,
};


// Queue item holding information about task to be processed.
struct QueueItem {
  inline QueueItem(RequestType requestType, String payload) {
    this->requestType = requestType;
    this->payload = payload;
  }
  inline QueueItem(RequestType requestType) { this->requestType = requestType; }
  inline QueueItem() {};

  // type of request to be processed
  RequestType requestType;
  // incoming payload that should be processed
  String payload;
};

// Implements asynchronous processing queue for the processing
// of incoming MQTT requests in the main loop
// The underlying internal queue implementation is based on Arduino-Queue
// https://raw.githubusercontent.com/sdesalas/Arduino-Queue.h/master/Queue.h
//
class ProcessingQueue {

public:
  //********  Static data members **********
  // maximum number of items in queue
  static const unsigned int MAX_SIZE;

public:
  ProcessingQueue();

  // Returns size of queue
  inline unsigned int count() { return queue.count(); }

  // try pushing a new item on the queue
  // Returns: true if push succeeded, false if could not push because queue is full or queue is locked
  bool try_push(const QueueItem& item);

  // try pop from queue.
  // Return: valid item if success, invalid item with requestType == 0 if can't pop
  //    due to empty queue or queue locked.
  QueueItem try_pop();

private:
  // queue holding items deferred for main loop processing
  ArduinoQueue<QueueItem> queue;

};


#endif  // _PROCESSINGQUEUE_H_
