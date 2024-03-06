#include "threadpool.h"
#include <iostream>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME =60;//单位s

//线程池构造函数
ThreadPool::ThreadPool()
	:initThreadSzie_(0)
	, taskSize_(0)
	, taskQueMaxThreshHold_(TASK_MAX_THRESHHOLD)
	, poolMode_(PoolMode::MODE_FIXED)
	, isPoolRunning_(false)
	, idelTheadsSize_(0)
	, threadSizeThreshHold_(THREAD_MAX_THRESHHOLD)
	, curThreadSize_(0)
{}

//线程析构函数
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//ThreadPool线程等待线程池内所有线程返回,1，没有任务阻塞 2，正在执行任务中
	//noEmpty_.notify_all();//唤醒所有等到在notEmpty.wait()上的线程
	std::unique_lock<std::mutex>lock(taskQueMtx_);
	noEmpty_.notify_all();//唤醒所有等到在notEmpty.wait()上的线程
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//检查Pool的运行状态
bool ThreadPool::checkRunningState()const
{
	return this->isPoolRunning_;
}

//设置线程池的工作模式
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//设置task任务队列数量上限阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshholdNums)
{
	if (checkRunningState())
		return;
	this->taskQueMaxThreshHold_ = threshholdNums;
}

//设置线程池cached模式下线程阈值
void ThreadPool::setThreadSizeThreshHold(int threshholdNums)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		this->threadSizeThreshHold_ = threshholdNums;
	}
	
}
//设置初始的线程数量
//void ThreadPool::setInitThreadSzie(int initThreadNums)

//给线程池提交任务，用户调用该接口，传入任务对象，生产任务
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//获取锁
	std::unique_lock<std::mutex>lock(taskQueMtx_);
	//线程的通信，等待任务队列有空余
	/*while (taskQue_.size() == taskQueMaxThreshHold_)
	{
		notFull_.wait(lock);
	}
	*/
	//如果队列已满，阻塞等待，并释放锁，否则继续向下执行
	//用户提交任务阻塞不能超过1s否则阻塞失败，返回
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//,wait_for();在1s内如果[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }满足立刻返回，
	// 或者阻塞超过1s返回
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		//not_Full_等待1s条件依然不满足
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp,false);//
	}
	//如果有空余，把任务放入任务队列

	taskQue_.emplace(sp);
	taskSize_++;
	//因为新放了任务，任务队列肯定不空了，notEmpty_上进行通知，赶快分配线程执行任务
	noEmpty_.notify_all();

	//cached模式，任务处理紧急，场景：小儿快的任务
	// 需要根据任务数量和空闲线程的数量判断是否需要创建新的线程
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idelTheadsSize_
		&&curThreadSize_<threadSizeThreshHold_)
	{
		std::cout << "创建新线程。。。"<< std::endl;
		//创建新线程对象
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this
		, std::placeholders::_1));
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start_();//启动线程
		//修改线程个数相关的变量
		curThreadSize_++;
		idelTheadsSize_++;
	}
	//返回任务的Result对象
	return Result(sp);
}

//开启线程池
void ThreadPool::start(int initThreadNums)
{
	//设置线程池的运行状态
	isPoolRunning_ = true;
	//记录初始线程个数
	this->initThreadSzie_ = initThreadNums;
	this->curThreadSize_ = initThreadNums;

	//创建线程对象
	for (int i = 0; i < initThreadSzie_; i++)
	{
		//创建thread线程对象时把线程函数给到thread线程对象
		std::unique_ptr<Thread> ptr= std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,
			std::placeholders::_1));
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
		//threads_.emplace_back(std::move(ptr));//右值引用
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	//启动所有线程,std::vector<std::thread*> threads_;	//线程列表
	for (int i = 0; i < initThreadSzie_; i++)
	{
		
		threads_[i]->start_();//执行一个线程函数
		idelTheadsSize_++;//每启动一个线程，空闲线程数量+1；
	}
}

//定义线程函数，执行任务	线程池的所有线程从任务队列里面消费任务
void ThreadPool::threadFunc(int threadid)
{
	//std::cout << "begin thradFunc()" << std::this_thread::get_id() << std::endl;
	//std::cout << "end thradFunc()" << std::this_thread::get_id() << std::endl;
	//一直循环去任务队列里面取任务
	while (isPoolRunning_)

	{//结束时，线程可能已经进入while，运行到此处
		auto lastTime = std::chrono::high_resolution_clock().now();//初始时间
		std::shared_ptr<Task> task;//
		{
			//获取锁
			std::unique_lock<std::mutex>lock(taskQueMtx_);//出作用域即析构释放锁
			std::cout << "尝试获取任务..." << std::this_thread::get_id() << std::endl;
			

			//在cached模式下，有可能已经创建了很多的线程但是空闲时间超过60s的话应该结束多余的线程进行回收
			//超过initThreadSize_数量的线程要进行回收
			// 当前时间-上一次线程执行时间>60s,回收线程
			
				//每一秒返回一次，怎么区分超时返回，还是有任务待执行返回
			///锁＋双重判断
				while (isPoolRunning_&&taskQue_.size()==0)//任务队列没任务，进入while等待条件变量唤醒
				{
					//条件变量超时返回，
					if (poolMode_ == PoolMode::MODE_CACHED)
					{

						if (std::cv_status::timeout ==
							noEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
							if (dur.count() > THREAD_MAX_IDLE_TIME
								&& curThreadSize_ > initThreadSzie_)
							{
								//开始回收当前线程
								//记录线程数量的值进行修改
								//把线程对象，从线程列表容器中删除,没有办法匹配treadFunc线程对象和vector容器中
								// thread线程对象
								//线程id-》线程对象》删除
								threads_.erase(threadid);
								curThreadSize_--;
								idelTheadsSize_--;
								std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
								return;
							}
						}
					}

					else
					{
						//等待notEmpty条件
						noEmpty_.wait(lock);
					}
					//线程池要结束，回收线程资源
					//if (!isPoolRunning_)
					//{
					//	threads_.erase(threadid);
					//	std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
					//	exitCond_.notify_all();//阻塞的线程
					//	return;//结束线程函数，结束当前线程
					//}
				}
				//线程池要结束，回收线程资源
				//线程池要结束，回收线程资源
				if (!isPoolRunning_)
				{
					//threads_.erase(threadid);
					//std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
					//exitCond_.notify_all();//阻塞的线程
					//return;//结束线程函数，结束当前线程
					break;
				}
		
			idelTheadsSize_--;//获取任务

			std::cout << "获取任务成功" << std::this_thread::get_id() << std::endl;
			//从任务队列中取一个任务出来
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//如果依然有剩余任务继续通知其他的线程执行任务
			if (taskQue_.size() > 0)
			{
				noEmpty_.notify_all();
			}

			//取出一个任务进行通知，通知可以继续提交生成任务
			notFull_.notify_all();
			//取完任务释放锁给予其他线程继续取任务
		}
		if (task != nullptr)
		{
			//当前线程负责执行这个任务
			//task->run();//执行任务；把任务的返回值通过setVal方法给到Result；
			task->exec();
		}
		
		idelTheadsSize_++;//任务处理完成
		lastTime = std::chrono::high_resolution_clock().now();//更新线程调度执行完任务的时间
	}

	threads_.erase(threadid);
	std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
	exitCond_.notify_all();//正在执行任务的线程
}
// Task()方法实现
Task::Task()
	:result_(nullptr)
{}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());//这里调用run（）发生多态
	}
	
}

void Task::setResult(Result* res)
{
	this->result_ = res;
}
///////线程方法实现

int Thread::generateId_ = 0;

//线程构造
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{

}

//线程析构
Thread::~Thread()
{

}

//启动线程
void Thread::start_()
{
	//创建一个线程来,执行线程函数
	std::thread t(func_,threadId_);//
	t.detach();//设置为分离线程
}
//获取线程id
int Thread::getId()const
{
	return threadId_;
}

/////Result方法的实现
Result::Result(std::shared_ptr<Task> task,bool isValid)
	:isValid_(isValid)
	,task_(task)
{
	task_->setResult(this);
}

//get方法，用户调用这个方法获取task的返回值
Any Result::get()//用户调用
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();//task任务如果没有执行完，这里会阻塞用户线程
	return std::move(any_);
}
////setVal方法，获取任务执行返回值的对象

void Result::setVal(Any any)
{
	//存储task的返回值
	this->any_ = std::move(any);
	sem_.post(); //已经获取了任务的返回值，增加信号量资源
}