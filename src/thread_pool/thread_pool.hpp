#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include <vector>
#include "locker.hpp"

namespace thread_space {
    using namespace locker_space;


    template<typename T>
    class thread_pool {    
    public:
        thread_pool(int thread_number = 8, int max_requests = 10000);
        ~thread_pool();
        bool append(T *request);    //向请求队列中添加任务

    private:
        static void* worker(void *arg); //工作线程
        void run();

    private:

        int m_thread_number;    //线程池中的线程数
        int m_max_requests; //请求队列中允许的最大请求数
        std::vector<pthread_t> m_threads;  //描述线程池的数组，大小为m_thread_number;
        std::list<T*> m_workqueue;  //请求队列
        locker m_queuelocker;   //请求队列的互斥锁
        sem m_queuestat;    //请求队列的信号量，判断是否有请求任务需要处理
        bool m_stop;    //是否结束线程
    };


    template<typename T>
    thread_pool<T>::thread_pool(int thread_number, int max_requests): 
            m_thread_number(thread_number), m_max_requests(max_requests),
            m_stop(false) {

                if (thread_number <= 0 || max_requests <= 0) {
                    throw std::exception();
                }

                m_threads.reserve(m_thread_number);

                for (int i = 0; i < m_thread_number; ++i) {
                    std::cout << "create the %dth thread\n" << i;
                    if (pthread_create(&m_threads[i], NULL, worker, this) != 0) {
                        throw std::exception()
                    }

                    if (pthread_detach(m_threads[i])) {
                        throw std::exception();
                    }
                }
            }

    template<typename T>
    thread_pool<T>::~thread_pool() {
        m_stop = true;
    }

    template<typename T>
    bool thread_pool<T>::append(T* request) {
        m_queuelocker.lock();
        if (m_workqueue.size() > m_max_requests) {
            m_queuelocker.unlock();
            return false;
        }

        m_workqueue.push_back(request);
        m_queuelocker.unlock;
        m_queuestat.post;

        return true;
    }

    template<typename T>
    void* thread_pool<T>::worker(void *arg) {
        thread_pool *pool = reinterpret_cast<thread_pool*>(arg);
        pool->run();
        return pool;
    }

    template<typename T>
    void thread_pool<T>::run() {
        while (!m_stop) {
            m_queuestat.wait();
            //有任务可获取，加锁并从任务队列中取出任务
            m_queuelocker.lock();
            if (m_workqueue.empty()) {
                m_queuelocker.unlock()
                continue;
            }

            T *request = m_workqueue.front();
            m_workqueue.pop_front();
            m_queuelocker.unlock();

            if (request == NULL) {
                continue;
            }

            //执行线程任务
            request->process();
        }
    }
}

#endif