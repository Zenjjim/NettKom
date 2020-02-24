#include <condition_variable>
#include <iostream>
#include <thread>
#include <list>
#include <functional>
#include <vector>
using namespace std;



class Workers{
private:
    vector<thread> worker_threads;
    list<function<void()>> tasks;
    mutex tasks_mutex;
    bool wait = true;
    mutex wait_mutex;
    condition_variable cv;
    int numberOfThreads = 1;
    bool stopped = false;
    mutex stopped_mutex;
public:
    Workers(int numberOfThreads1){
        numberOfThreads = numberOfThreads1;
    }

    void post(function<void()> f){
        {
            lock_guard<mutex> lock(tasks_mutex);
            tasks.emplace_back(f);
        }
        {
            lock_guard<mutex> lock(wait_mutex);
            wait = false;
        }
        cv.notify_all();
    }

    void post_timeout(function<void()> f, int ms){
        thread t([&ms, &f](){
            this_thread::sleep_for(std::chrono::milliseconds(ms));
            f();
        });
        t.join();
    }

    void start(){
        worker_threads.clear();
        for(int i = 0; i<numberOfThreads; i++){
            worker_threads.emplace_back([this]{
                bool tasksLeft = false;
                bool done = false;
                while(!done){
                    function<void()> task;
                    if(!tasksLeft){
                        unique_lock<mutex> lock(wait_mutex);
                        while(wait){
                            cv.wait(lock);
                        }
                    }
                    {
                        lock_guard<mutex> lock(tasks_mutex);
                        if(!tasks.empty()){
                            task = *tasks.begin();
                            tasks.pop_front();
                            tasksLeft = true;
                        }
                        else{
                            {
                                unique_lock<mutex> lock(stopped_mutex);
                                if(stopped){
                                    done = true;
                                }else{
                                    unique_lock<mutex> lock(wait_mutex);
                                    wait = true;
                                    tasksLeft = false;
                                }
                            }
                        }
                    }
                    if(task){
                        task();
                        cv.notify_one();
                    }
                }
            });
        }
    }
    void stop(){
        {
            unique_lock<mutex> lock(stopped_mutex);
            stopped = true;
        }
        {
            lock_guard<mutex> lock(wait_mutex);
            wait = false;
        }
        cv.notify_all();
        for (auto &thread: worker_threads){
            thread.join();
        }
        cout<<"all threads stopped"<<endl;
    }
};




int main(){
    Workers workers(100);
    Workers event_loop(1);

    workers.post_timeout([](){
        cout<<"yo after 1 sec"<<endl;
    },1000);

    workers.start();
    event_loop.start();
    for(int i = 0; i<1000; i++){
            workers.post([](){
                int sum = 0;
                for (int i = 0; i<1000000; i++){
                    sum+=i;
                }
                cout<<"task finished: "<<sum<<endl;
            });
            event_loop.post([](){
                int sum = 0;
                for (int i = 0; i<1000000; i++){
                    sum+=i;
                }
                cout<<"task finished in event loop: "<<sum<<endl;
            });
    }
    event_loop.stop();
    workers.stop();
}