/*
* 本程序使用c++11/c++14/c++20实现的线程池项目，并编为动态库的形式
*/
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <thread>
#include <list>//不是线程安全的
#include <mutex>
#include <queue>//不是线程安全的
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_map>
//Any类型可以接受任意的数据类型
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const  Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;
	template <typename T>
	//这个构造函数可以让Any类型接受任意其他类型的数据
	Any(T data)
		:base_(std::make_unique<Derive<T>>(data))
	{
	}	
	//这个方法可以提取Any对象里面的data数据
	template <typename T>
	T cast_()
	{
		//怎么从base_找到它所知的派生类Derive对象，从它里面提取出data成员变量
		//基类指针》派生类指针 RTTI
		Derive <T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
				throw"type is incompatiable!!!";
		}
			return pd->data_;
	}
	private:
	//基类类型
	class Base
	{
	public:
		virtual~Base() = default;
	private:
	};
	//派生类类型
	template <typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data)//构造函数
		{}
		T data_;
	};
	private:
		//定义一个基类指针
		std::unique_ptr <Base> base_;
	};
		//任务抽象基类
	

	//实现一个信号量类型
	class Semaphore
	{
	public:
			Semaphore(int Limit = 0)
				:resLimit_(Limit)
			{}
			~Semaphore() = default;
	//获取一个信号量资源
		void wait()
		{
			std::unique_lock<std::mutex>lock(mtx_);
			//等待信号量有资源，没有资源的话会阻塞当前线程
			cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
			resLimit_--;
		}
		//增加一个信号量资源
		void post()
		{
			std::unique_lock<std::mutex>lock(mtx_);
			resLimit_++;
			//Linux下 条件变量的析构函数什么也没做，导致这里状态已经失效，无故阻塞
			cond_.notify_all();
		}
	private:
			int resLimit_;//资源计数
			std::mutex mtx_;//互斥锁
			std::condition_variable cond_;//条件变量
	};
	//实现接收提交任务到现场池的taske任务执行完成后的返回值类型，Result


	class Task;//Task类型的前置声明
	class Result
	{
	public:
	Result(std::shared_ptr<Task> task,bool isValid=true);
	~Result() = default;
	//setVal方法，获取任务执行返回值的对象
	void setVal(Any any);

	//get方法，用户调用这个方法获取task的返回值
	Any get();
	private:
	Any any_;//存储任务的返回值
	Semaphore sem_;//线程通信信号量
	std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象
	std::atomic_bool isValid_;//返回值是否有效
	};

	class Task
	{
	public:
		void exec();
		Task();
		~Task() = default;
		//用户可以自定义任意任务类型，从Task继承，重写run方法，实现自定义任务处理
		virtual Any run() = 0;//纯虚函数
		void setResult(Result* res);
	private:
		Result* result_;//result对象的生命周期长于Task
	};

	//线程池支持的模式
	enum class PoolMode
	{
		MODE_FIXED,//固定数量的线程池
		MODE_CACHED,//动态数量的线程池
	};
	//线程类型
	class Thread
	{
	public:
		//线程函数对象类型
		using  ThreadFunc = std::function<void(int)>;
		//线程构造
		Thread(ThreadFunc func);
		//线程析构
		~Thread();
		//启动线程
		void start_();
		//获取线程id
		int getId()const;
	private:
		ThreadFunc func_;//线程函数对象
		static int generateId_;
		int threadId_;//保存线程id
	};
	/*
		example:
		ThreadPool pool;
		pool.start(4);
		
		class My Task:Public Task
		{
			public:
				void run(){//code...}
		}

		pool.submitTask(std::make_shared<Mytask>());
	*/
	//线程池类型
	class ThreadPool
	{
	public:
		//线程池构造
		ThreadPool();
		//线程池析构
		~ThreadPool();

		//开启线程池
		void start(int intThreadNums = std::thread::hardware_concurrency());

		//设置线程池的工作模式,缺省为MODE_FIXED
		void setMode(PoolMode mode);

		//设置task任务队列数量上限阈值
		void setTaskQueMaxThreshHold(int threshholdNums);

		//设置线程池cached模式线程上限阈值
		void setThreadSizeThreshHold(int threshholdNums);
	
		//设置初始的线程数量
		//void setInitThreadSzie(int initThreadNums);
		//给线程池提交任务
		Result submitTask(std::shared_ptr<Task> sp);
		ThreadPool(const ThreadPool&) = delete;//禁止拷贝构造
		ThreadPool& operator = (const ThreadPool&) = delete;//禁止赋值
	private:
		//定义线程函数
		void threadFunc(int treadid);
		//检查Pool的运行状态
		bool checkRunningState()const;
	private:
		//std::vector<std::unique_ptr<Thread>>threads_;	//线程列表
		std::unordered_map<int,std::unique_ptr<Thread>>threads_;	//线程列表

		int initThreadSzie_;				//初始线程数量
		std::atomic_int curThreadSize_;	//记录当前线程池的总数量
		int threadSizeThreshHold_;			//线程数量上限阈值
		std::atomic_int idelTheadsSize_;			//记录空闲线程的数量

		PoolMode poolMode_;						//记录线程池的工作模式
		std::atomic_bool isPoolRunning_;	//表示当前线程池的启动状态

		//std::queue<Task*> queTasks;
		std::queue<std::shared_ptr<Task>>taskQue_;//任务队列
		std::atomic_int taskSize_;				//任务数量
		int taskQueMaxThreshHold_;		//任务队列上限阈值

		std::mutex taskQueMtx_;					//保证任务队列的线程安全
		std::condition_variable notFull_;		//任务队列不满
		std::condition_variable noEmpty_;		//任务队列不空
		std::condition_variable exitCond_;		//等待线程资源全部回收
	};

#endif

