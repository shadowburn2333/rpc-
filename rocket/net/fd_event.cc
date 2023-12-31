#include "fd_event.h"
#include "../common/log.h"
#include<string.h>
namespace rocket{

FdEvent::FdEvent(int fd):m_fd(fd){

}

FdEvent::~FdEvent(){

}

std::function<void()> FdEvent::handler(TriggerEvent event){
    if(event==TriggerEvent::IN_EVENT){
       return m_read_callback;
    }
    else{
        return m_write_callback;
    }
}

void FdEvent::listen(TriggerEvent event_type,std::function<void()>callback){
    
    if(event_type==TriggerEvent::IN_EVENT){

        m_listen_events.events|=EPOLLIN;
        m_read_callback=callback;
        m_listen_events.data.ptr=this;
    }
    else{
        
        m_listen_events.events|=EPOLLOUT;
        m_read_callback=callback;   
    }
    
        m_listen_events.data.ptr=this;
}



}
