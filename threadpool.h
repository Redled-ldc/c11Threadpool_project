/*
* ������ʹ��c++11/c++14/c++20ʵ�ֵ��̳߳���Ŀ������Ϊ��̬�����ʽ
*/
#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <iostream>
#include <thread>
#include <list>//�����̰߳�ȫ��
#include <mutex>
#include <queue>//�����̰߳�ȫ��
#include <condition_variable>
#include <vector>
#include <memory>
#include <atomic>
#include <functional>
#include <unordered_map>
//Any���Ϳ��Խ����������������
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
	//������캯��������Any���ͽ��������������͵�����
	Any(T data)
		:base_(std::make_unique<Derive<T>>(data))
	{
	}	
	//�������������ȡAny���������data����
	template <typename T>
	T cast_()
	{
		//��ô��base_�ҵ�����֪��������Derive���󣬴���������ȡ��data��Ա����
		//����ָ�롷������ָ�� RTTI
		Derive <T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
				throw"type is incompatiable!!!";
		}
			return pd->data_;
	}
	private:
	//��������
	class Base
	{
	public:
		virtual~Base() = default;
	private:
	};
	//����������
	template <typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data)//���캯��
		{}
		T data_;
	};
	private:
		//����һ������ָ��
		std::unique_ptr <Base> base_;
	};
		//����������
	

	//ʵ��һ���ź�������
	class Semaphore
	{
	public:
			Semaphore(int Limit = 0)
				:resLimit_(Limit)
			{}
			~Semaphore() = default;
	//��ȡһ���ź�����Դ
		void wait()
		{
			std::unique_lock<std::mutex>lock(mtx_);
			//�ȴ��ź�������Դ��û����Դ�Ļ���������ǰ�߳�
			cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });
			resLimit_--;
		}
		//����һ���ź�����Դ
		void post()
		{
			std::unique_lock<std::mutex>lock(mtx_);
			resLimit_++;
			//Linux�� ������������������ʲôҲû������������״̬�Ѿ�ʧЧ���޹�����
			cond_.notify_all();
		}
	private:
			int resLimit_;//��Դ����
			std::mutex mtx_;//������
			std::condition_variable cond_;//��������
	};
	//ʵ�ֽ����ύ�����ֳ��ص�taske����ִ����ɺ�ķ���ֵ���ͣ�Result


	class Task;//Task���͵�ǰ������
	class Result
	{
	public:
	Result(std::shared_ptr<Task> task,bool isValid=true);
	~Result() = default;
	//setVal��������ȡ����ִ�з���ֵ�Ķ���
	void setVal(Any any);

	//get�������û��������������ȡtask�ķ���ֵ
	Any get();
	private:
	Any any_;//�洢����ķ���ֵ
	Semaphore sem_;//�߳�ͨ���ź���
	std::shared_ptr<Task> task_;//ָ���Ӧ��ȡ����ֵ���������
	std::atomic_bool isValid_;//����ֵ�Ƿ���Ч
	};

	class Task
	{
	public:
		void exec();
		Task();
		~Task() = default;
		//�û������Զ��������������ͣ���Task�̳У���дrun������ʵ���Զ���������
		virtual Any run() = 0;//���麯��
		void setResult(Result* res);
	private:
		Result* result_;//result������������ڳ���Task
	};

	//�̳߳�֧�ֵ�ģʽ
	enum class PoolMode
	{
		MODE_FIXED,//�̶��������̳߳�
		MODE_CACHED,//��̬�������̳߳�
	};
	//�߳�����
	class Thread
	{
	public:
		//�̺߳�����������
		using  ThreadFunc = std::function<void(int)>;
		//�̹߳���
		Thread(ThreadFunc func);
		//�߳�����
		~Thread();
		//�����߳�
		void start_();
		//��ȡ�߳�id
		int getId()const;
	private:
		ThreadFunc func_;//�̺߳�������
		static int generateId_;
		int threadId_;//�����߳�id
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
	//�̳߳�����
	class ThreadPool
	{
	public:
		//�̳߳ع���
		ThreadPool();
		//�̳߳�����
		~ThreadPool();

		//�����̳߳�
		void start(int intThreadNums = std::thread::hardware_concurrency());

		//�����̳߳صĹ���ģʽ,ȱʡΪMODE_FIXED
		void setMode(PoolMode mode);

		//����task�����������������ֵ
		void setTaskQueMaxThreshHold(int threshholdNums);

		//�����̳߳�cachedģʽ�߳�������ֵ
		void setThreadSizeThreshHold(int threshholdNums);
	
		//���ó�ʼ���߳�����
		//void setInitThreadSzie(int initThreadNums);
		//���̳߳��ύ����
		Result submitTask(std::shared_ptr<Task> sp);
		ThreadPool(const ThreadPool&) = delete;//��ֹ��������
		ThreadPool& operator = (const ThreadPool&) = delete;//��ֹ��ֵ
	private:
		//�����̺߳���
		void threadFunc(int treadid);
		//���Pool������״̬
		bool checkRunningState()const;
	private:
		//std::vector<std::unique_ptr<Thread>>threads_;	//�߳��б�
		std::unordered_map<int,std::unique_ptr<Thread>>threads_;	//�߳��б�

		int initThreadSzie_;				//��ʼ�߳�����
		std::atomic_int curThreadSize_;	//��¼��ǰ�̳߳ص�������
		int threadSizeThreshHold_;			//�߳�����������ֵ
		std::atomic_int idelTheadsSize_;			//��¼�����̵߳�����

		PoolMode poolMode_;						//��¼�̳߳صĹ���ģʽ
		std::atomic_bool isPoolRunning_;	//��ʾ��ǰ�̳߳ص�����״̬

		//std::queue<Task*> queTasks;
		std::queue<std::shared_ptr<Task>>taskQue_;//�������
		std::atomic_int taskSize_;				//��������
		int taskQueMaxThreshHold_;		//�������������ֵ

		std::mutex taskQueMtx_;					//��֤������е��̰߳�ȫ
		std::condition_variable notFull_;		//������в���
		std::condition_variable noEmpty_;		//������в���
		std::condition_variable exitCond_;		//�ȴ��߳���Դȫ������
	};

#endif

