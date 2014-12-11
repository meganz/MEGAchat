#ifndef CSERVICESTHREAD_H
#define CSERVICESTHREAD_H

#ifdef _WIN32
typedef DWORD t_svc_thread_id;
typedef HANDLE t_svc_thread_handle;
typedef unsigned __stdcall (*t_services_thread_func)(PVOID args);
static inline int svc_thread_start(void* args, t_svc_thread_handle* thread,
                                   t_svc_thread_id* id, t_svc_thread_func func)
{
    *handle = _beginthreadex(NULL, 0, func, args, 0, id);
    return (*handle != 0);
}
#define SVC_THREAD_FUNCDECL(func) unsigned __stdcall func(PVOID args)
typedef int t_svc_thread_funcret;
static inline void svc_thread_join(t_svc_thread_handle thread)
{
    WaitForSingleObject(thread, INFINITE);
}
static inline svc_threads_ids_equal(t_svc_thread_id lhs, t_svc_thread_id rhs)
{
    return (lhs == rhs);
}
#else //POSIX threads
typedef pthread_t t_svc_thread_id;
typedef pthread_t t_svc_thread_handle;
typedef void*(*t_svc_thread_func)(void*);
static inline int svc_thread_start(void* args, t_svc_thread_handle* thread,
                                   t_svc_thread_id* id, t_svc_thread_func func)
{
    if (pthread_create(thread, NULL, func, args) == 0)
        return 0;
    *id = *thread;
    return 1;
}

#define SVC_THREAD_FUNCDECL(func) void* func(void *args)
typedef void* t_svc_thread_funcret;
static inline void svc_thread_join(t_svc_thread_handle thread)
{
    pthread_join(thread, NULL);
}
static inline int svc_thread_ids_equal(t_svc_thread_id lhs, t_svc_thread_id rhs)
{
    return pthread_equal(lhs, rhs);
}
#endif

#endif // CSERVICESTHREAD_H
