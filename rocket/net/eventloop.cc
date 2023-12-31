#include "eventloop.h"
#include "../common/util.h"
#include<sys/socket.h>
#include"../common/log.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include<string.h>


#define ADD_TO_EPOLL()\
    auto it = m_listen_fds.find(event->getFd());\
           int op=EPOLL_CTL_ADD;\
           if(it!=m_listen_fds.end()){\
               op=EPOLL_CTL_MOD;\
           }\
           epoll_event tmp=event->getEpollEvent();\
           int rt=epoll_ctl(m_epoll_fd,op,event->getFd(),&tmp);\
           if(rt==-1){\
               ERRORLOG("failed epoll_ctl when add fd,errno=%d, error=%s",errno,strerror(errno));\
           }\
           DEBUGLOG("add event success,fd[%d]",event->getFd())\


#define DELETE_TO_EPOLL()\
        auto it = m_listen_fds.find(event->getFd());\
        if(it==m_listen_fds.end()){\
            return;\
        }\
        int op=EPOLL_CTL_DEL;\
        epoll_event tmp=event->getEpollEvent();\
        int rt=epoll_ctl(m_epoll_fd,op,event->getFd(),&tmp);\
        if(rt==-1){\
ERRORLOG("failed epoll_ctl when add fd,errno=%d, error=%s",errno,strerror(errno));\
}\
    DEBUGLOG("add event success,fd[%d]",event->getFd())\

namespace rocket{

static thread_local EventLoop * t_current_eventloop=NULL;
static int g_epoll_max_timeout=10000;
 static int g_epoll_max_events=10;

EventLoop::EventLoop(){
    if(t_current_eventloop!=NULL){
        ERRORLOG("failed to create event loop,this thread has created event loop");
        exit(0);
    }
     m_thread_pid=getThreadId();


     m_epoll_fd = epoll_create(10);

    if(m_epoll_fd==-1){
         
        ERRORLOG("failed to create event loop,epoll_create error,error_info[%d]",errno);
        exit(0);
    }

    

    initWakeUpFdEvent();
    INFOLOG("succ create event loop in thread %d",m_thread_pid);
    
    t_current_eventloop=this;

}
EventLoop:: ~EventLoop(){
    close(m_epoll_fd);
    if(m_wakeup_fd_event){
        delete m_wakeup_fd_event;
        m_wakeup_fd_event=NULL;
    }
}


void EventLoop:: initWakeUpFdEvent(){

    m_wakeup_fd=eventfd(0,EFD_NONBLOCK);
    
    if(m_wakeup_fd<0){
            
        ERRORLOG("failed to create event loop,eventfd create error,error_info[%d]",errno);
        exit(0);
  }
    m_wakeup_fd_event=new WakeUpFdEvent(m_wakeup_fd);
    m_wakeup_fd_event->listen(FdEvent::IN_EVENT,[this](){
          char buf[8];
          while(read(m_wakeup_fd,buf,8)!=-1&&errno!=EAGAIN){
  
          }                                                                            
          DEBUGLOG("read full bytes from wakeup fd[%d]",m_wakeup_fd);
  
      });

    addEpollEvent(m_wakeup_fd_event);
}

void EventLoop:: loop(){
    while(!m_stop_flag){
        ScopeMutext<Mutex>lock(m_mutex);
        std::queue<std::function<void()>> tmp_tasks=m_pending_tasks;
        m_pending_tasks.swap(tmp_tasks);
        lock.unlock();
        while(!tmp_tasks.empty()){
            tmp_tasks.front()();
            tmp_tasks.pop();
        }


        int timeout=g_epoll_max_timeout;

        epoll_event result_events[g_epoll_max_events];
        int rt=epoll_wait(m_epoll_fd,result_events,g_epoll_max_events,timeout);

        if(rt<0){
            ERRORLOG("epoll_wait error,error",errno);
        }else{
            for(int i=0;i<rt;++i){
                epoll_event trigger_event=result_events[i];
                FdEvent* fd_event=static_cast<FdEvent*>(trigger_event.data.ptr);                
                if(fd_event==NULL){
                    continue;
                }
                if(trigger_event.events|EPOLLIN){
                    addTask(fd_event->handler(FdEvent::IN_EVENT));
                }

                if(trigger_event.events | EPOLLOUT){
                    addTask(fd_event->handler(FdEvent::OUT_EVENT));
                }

            }
        }


    }
}
  
void EventLoop:: wakeup(){
    m_wakeup_fd_event->wakeup();        
}
  
void EventLoop::stop(){
    m_stop_flag=true;
}

void EventLoop::dealMakeup(){
    
}


void EventLoop::addEpollEvent(FdEvent *event){
    if(isInLoopThread()){
       ADD_TO_EPOLL();
    }
    else{
        auto cb=[this,event](){
            ADD_TO_EPOLL();
        };

        addTask(cb,true);
    }

}
void EventLoop::addTask(std::function<void()> cb,bool is_wake_up){
    ScopeMutext<Mutex> lock(m_mutex);
    m_pending_tasks.push(cb);
    lock.unlock();

    if(is_wake_up){
        wakeup();
    }
}


void EventLoop::deleteEpollEvent(FdEvent *event){
    if(isInLoopThread()){
        DELETE_TO_EPOLL();
    }
    else{
        auto cb=[this,event](){
            DELETE_TO_EPOLL();
        };

        addTask(cb,true);
    }

}

bool EventLoop::isInLoopThread(){
    return getThreadId()==m_thread_pid;
}





}
