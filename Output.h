/*
    microsoft-oms-auditd-plugin

    Copyright (c) Microsoft Corporation

    All rights reserved.

    MIT License

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the ""Software""), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef AUOMS_OUTPUT_H
#define AUOMS_OUTPUT_H

#include "RunBase.h"
#include "Queue.h"
#include "Config.h"
#include "EventId.h"
#include "OMSEventWriter.h"
#include "IO.h"

#include <string>
#include <mutex>
#include <memory>
#include <vector>

/****************************************************************************
 *
 ****************************************************************************/

class AckQueue {
public:
    AckQueue(size_t max_size);

    size_t MaxSize() {
        return _max_size;
    }

    void Close();

    bool Add(const EventId& event_id, const QueueCursor& cursor);

    // Returns false on timeout, true is queue is empty
    bool Wait(int millis);

    bool Ack(const EventId& event_id, QueueCursor& cursor);

private:
    std::mutex _mutex;
    std::condition_variable _cond;
    std::vector<std::pair<EventId, QueueCursor>> _ring;
    size_t _max_size;
    bool _closed;
    size_t _head;
    size_t _tail;
    size_t _size;
};

/****************************************************************************
 *
 ****************************************************************************/

class CursorWriter: public RunBase {
public:

    CursorWriter(const std::string& name, const std::string& path): _name(name), _path(path), _cursor_updated(false)
    {}

    bool Read();
    bool Write();
    bool Delete();

    QueueCursor GetCursor();
    void UpdateCursor(const QueueCursor& cursor);

protected:
    virtual void on_stopping();
    virtual void run();

private:
    std::string _name;
    std::string _path;
    std::mutex _mutex;
    std::condition_variable _cond;
    bool _cursor_updated;
    QueueCursor _cursor;
};

/****************************************************************************
 *
 ****************************************************************************/

class Output;

class AckReader: public RunBase {
public:

    AckReader(const std::string& name): _name(name)
    {}

    void Init(std::shared_ptr<IEventWriter> event_writer,
              std::shared_ptr<IOBase> writer,
              std::shared_ptr<AckQueue> ack_queue,
              std::shared_ptr<CursorWriter> cursor_writer);

protected:
    virtual void run();

    std::string _name;
    std::shared_ptr<IEventWriter> _event_writer;
    std::shared_ptr<IOBase> _writer;
    std::shared_ptr<AckQueue> _queue;
    std::shared_ptr<CursorWriter> _cursor_writer;
};

/****************************************************************************
 *
 ****************************************************************************/

class Output: public RunBase {
public:
    static constexpr int START_SLEEP_PERIOD = 1;
    static constexpr int MAX_SLEEP_PERIOD = 60;
    static constexpr int DEFAULT_ACK_QUEUE_SIZE = 1000;

    Output(const std::string& name, const std::string& cursor_path, std::shared_ptr<Queue>& queue):
            _name(name), _cursor_path(cursor_path), _queue(queue), _ack_mode(false)
    {
        _cursor_writer = std::make_shared<CursorWriter>(name, cursor_path);
        _ack_reader = std::unique_ptr<AckReader>(new AckReader(name));
    }

    bool IsConfigDifferent(const Config& config);

    // Return false if load failed
    bool Load(std::unique_ptr<Config>& config);

    // Delete any resources associated with the output
    void Delete();

protected:
    friend class AckReader;

    virtual void on_stopping();
    virtual void on_stop();
    virtual void run();

    // Return true on success, false if Output should stop.
    bool check_open();

    // Return true if writer closed and Output should reconnect, false if Output should stop.
    bool handle_events();

    std::mutex _mutex;
    std::string _name;
    std::string _cursor_path;
    std::string _socket_path;
    std::shared_ptr<Queue> _queue;
    bool _ack_mode;
    std::unique_ptr<Config> _config;
    QueueCursor _cursor;
    QueueCursor _ack_cursor;
    std::shared_ptr<IEventWriter> _event_writer;
    std::shared_ptr<IOBase> _writer;
    std::shared_ptr<AckQueue> _ack_queue;
    std::unique_ptr<AckReader> _ack_reader;
    std::shared_ptr<CursorWriter> _cursor_writer;
};


#endif //AUOMS_OUTPUT_H
