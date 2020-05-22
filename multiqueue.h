#ifndef MULTIQUEUE_MULTIQUEUE_H
#define MULTIQUEUE_MULTIQUEUE_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <iostream>
#include <random>
#include <cstring>
#include <cstdint>
#include <cstdlib>

template <class T>
class ReservablePriorityQueue : public std::priority_queue<T, std::vector<T>>
{
public:
    explicit ReservablePriorityQueue(std::size_t reserve_size) {
        this->c.reserve(reserve_size);
    }
};

template <class T>
class LockablePriorityQueueWithEmptyElement {
private:
    ReservablePriorityQueue<T> queue;
    const T empty_element;
public:
    explicit LockablePriorityQueueWithEmptyElement(std::size_t reserve_size, const T empty_element) :
            queue(ReservablePriorityQueue<T>(reserve_size)), empty_element(empty_element) {}

    LockablePriorityQueueWithEmptyElement(const LockablePriorityQueueWithEmptyElement & o) :
            queue(ReservablePriorityQueue<T>(512)), empty_element(empty_element) {}

    std::mutex mutex;

    void push(T value) {
        queue.push(value);
    }

    const T & top() const {
        if (queue.empty()) {
            return empty_element;
        }
        return queue.top();
    }

    T pop() {
        if (queue.empty()) {
            return empty_element;
        }
        T elem = queue.top();
        queue.pop();
        return elem;
    }

    std::size_t size() const {
        return queue.size();
    }
};

template<class T>
class Multiqueue {
private:
    std::vector<LockablePriorityQueueWithEmptyElement<T>> queues;
    const std::size_t num_queues;
    std::atomic<std::size_t> num_non_empty_queues;
    T empty_element;
    std::random_device dev;
    std::mt19937 rng;
    std::uniform_int_distribution<std::mt19937::result_type> dist; // []
    std::size_t gen_random_queue_index() {
        return dist(rng);
    }
    std::atomic<std::size_t> num_pushes;
    std::vector<std::size_t> max_queue_sizes;
public:
    Multiqueue(int num_threads, int size_multiple, T empty_element, std::size_t one_queue_reserve_size = 512) :
            num_queues(std::max(2, num_threads * size_multiple)), num_non_empty_queues(0),
            empty_element(empty_element), rng(dev()), dist(0, num_queues - 1), num_pushes(0),
            max_queue_sizes(num_queues, 0) {
        queues.reserve(num_queues);
        for (std::size_t i = 0; i < num_queues; i++) {
            queues.emplace_back(one_queue_reserve_size, empty_element);
        }
    }
    void push(T value) {
        LockablePriorityQueueWithEmptyElement<T> * q_ptr;
        std::size_t i;
        do {
            i = gen_random_queue_index();
            q_ptr = &queues[i];
        } while (!q_ptr->mutex.try_lock());
        auto & q = *q_ptr;
        if (q.top() == empty_element) {
            num_non_empty_queues++;
        }
        q.push(value);
        max_queue_sizes[i] = std::max(max_queue_sizes[i], q.size());
        q.mutex.unlock();
        num_pushes++;
    }
    std::size_t get_num_pushes() const {
        return num_pushes;
    }
    const std::vector<std::size_t> & get_max_queue_sizes() const {
        return max_queue_sizes;
    }
    T pop() {
        while (true) {
            if (num_non_empty_queues == 0) {
                return empty_element;
            }

            LockablePriorityQueueWithEmptyElement<T> * q1_ptr;
            LockablePriorityQueueWithEmptyElement<T> * q2_ptr;
            std::size_t i, j;
            do {
                do {
                    i = gen_random_queue_index();
                } while (i == num_queues - 1);
                q1_ptr = &queues[i];
            } while (!q1_ptr->mutex.try_lock());
            do {
                // lock in the increasing order to avoid the ABA problem
                do {
                    j = gen_random_queue_index();
                } while (j <= i);
                q2_ptr = &queues[j];
            } while (!q2_ptr->mutex.try_lock());

            auto & q1 = *q1_ptr;
            auto & q2 = *q2_ptr;

            T e1 = q1.top();
            T e2 = q2.top();

            if (e1 == empty_element && e2 == empty_element) {
                q1.mutex.unlock();
                q2.mutex.unlock();
                continue;
            }

            LockablePriorityQueueWithEmptyElement<T> * q_ptr;
            // reversed comparator because std::priority_queue is a max queue
            if (e1 == empty_element || (e2 != empty_element && e1 < e2)) {
                q1.mutex.unlock();
                q_ptr = &q2;
            } else {
                q2.mutex.unlock();
                q_ptr = &q1;
            }
            auto & q = *q_ptr;

            T e = q.pop();
            if (q.top() == empty_element) {
                num_non_empty_queues--;
            }
            q.mutex.unlock();
            return e;
        }
    }
};

#endif //MULTIQUEUE_MULTIQUEUE_H