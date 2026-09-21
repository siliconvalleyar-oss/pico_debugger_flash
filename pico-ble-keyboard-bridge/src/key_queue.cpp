#include "key_queue.h"

KeyQueue::KeyQueue() : head_(0), tail_(0), count_(0) {}

bool KeyQueue::push(const KeyEvent &ev) {
    if (count_ >= kCapacity) {
        // Cola llena: descartamos el evento nuevo en lugar de bloquear
        // o sobrescribir eventos pendientes (evita "comerse" texto viejo).
        return false;
    }
    buffer_[head_] = ev;
    head_ = (head_ + 1) % kCapacity;
    count_++;
    return true;
}

bool KeyQueue::pop(KeyEvent &out) {
    if (count_ == 0) {
        return false;
    }
    out = buffer_[tail_];
    tail_ = (tail_ + 1) % kCapacity;
    count_--;
    return true;
}

bool KeyQueue::empty() const { return count_ == 0; }
bool KeyQueue::full() const { return count_ >= kCapacity; }
size_t KeyQueue::size() const { return count_; }
