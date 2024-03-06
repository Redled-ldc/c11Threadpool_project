#include "threadpool.h"
#include <iostream>
const int TASK_MAX_THRESHHOLD = INT32_MAX;
const int THREAD_MAX_THRESHHOLD = 1024;
const int THREAD_MAX_IDLE_TIME =60;//��λs

//�̳߳ع��캯��
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

//�߳���������
ThreadPool::~ThreadPool()
{
	isPoolRunning_ = false;
	//ThreadPool�̵߳ȴ��̳߳��������̷߳���,1��û���������� 2������ִ��������
	//noEmpty_.notify_all();//�������еȵ���notEmpty.wait()�ϵ��߳�
	std::unique_lock<std::mutex>lock(taskQueMtx_);
	noEmpty_.notify_all();//�������еȵ���notEmpty.wait()�ϵ��߳�
	exitCond_.wait(lock, [&]()->bool {return threads_.size() == 0; });
}

//���Pool������״̬
bool ThreadPool::checkRunningState()const
{
	return this->isPoolRunning_;
}

//�����̳߳صĹ���ģʽ
void ThreadPool::setMode(PoolMode mode)
{
	if (checkRunningState())
		return;
	poolMode_ = mode;
}

//����task�����������������ֵ
void ThreadPool::setTaskQueMaxThreshHold(int threshholdNums)
{
	if (checkRunningState())
		return;
	this->taskQueMaxThreshHold_ = threshholdNums;
}

//�����̳߳�cachedģʽ���߳���ֵ
void ThreadPool::setThreadSizeThreshHold(int threshholdNums)
{
	if (checkRunningState())
		return;
	if (poolMode_ == PoolMode::MODE_CACHED)
	{
		this->threadSizeThreshHold_ = threshholdNums;
	}
	
}
//���ó�ʼ���߳�����
//void ThreadPool::setInitThreadSzie(int initThreadNums)

//���̳߳��ύ�����û����øýӿڣ��������������������
Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	//��ȡ��
	std::unique_lock<std::mutex>lock(taskQueMtx_);
	//�̵߳�ͨ�ţ��ȴ���������п���
	/*while (taskQue_.size() == taskQueMaxThreshHold_)
	{
		notFull_.wait(lock);
	}
	*/
	//������������������ȴ������ͷ����������������ִ��
	//�û��ύ�����������ܳ���1s��������ʧ�ܣ�����
	//notFull_.wait(lock, [&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; });
	//,wait_for();��1s�����[&]()->bool {return taskQue_.size() < taskQueMaxThreshHold_; }�������̷��أ�
	// ������������1s����
	if (!notFull_.wait_for(lock, std::chrono::seconds(1),
		[&]()->bool {return taskQue_.size() < (size_t)taskQueMaxThreshHold_; }))
	{
		//not_Full_�ȴ�1s������Ȼ������
		std::cerr << "task queue is full,submit task fail." << std::endl;
		return Result(sp,false);//
	}
	//����п��࣬����������������

	taskQue_.emplace(sp);
	taskSize_++;
	//��Ϊ�·�������������п϶������ˣ�notEmpty_�Ͻ���֪ͨ���Ͽ�����߳�ִ������
	noEmpty_.notify_all();

	//cachedģʽ�������������������С���������
	// ��Ҫ�������������Ϳ����̵߳������ж��Ƿ���Ҫ�����µ��߳�
	if (poolMode_ == PoolMode::MODE_CACHED
		&& taskSize_ > idelTheadsSize_
		&&curThreadSize_<threadSizeThreshHold_)
	{
		std::cout << "�������̡߳�����"<< std::endl;
		//�������̶߳���
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this
		, std::placeholders::_1));
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
		threads_[threadId]->start_();//�����߳�
		//�޸��̸߳�����صı���
		curThreadSize_++;
		idelTheadsSize_++;
	}
	//���������Result����
	return Result(sp);
}

//�����̳߳�
void ThreadPool::start(int initThreadNums)
{
	//�����̳߳ص�����״̬
	isPoolRunning_ = true;
	//��¼��ʼ�̸߳���
	this->initThreadSzie_ = initThreadNums;
	this->curThreadSize_ = initThreadNums;

	//�����̶߳���
	for (int i = 0; i < initThreadSzie_; i++)
	{
		//����thread�̶߳���ʱ���̺߳�������thread�̶߳���
		std::unique_ptr<Thread> ptr= std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this,
			std::placeholders::_1));
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc,this)));
		//threads_.emplace_back(std::move(ptr));//��ֵ����
		int threadId = ptr->getId();
		threads_.emplace(threadId, std::move(ptr));
	}

	//���������߳�,std::vector<std::thread*> threads_;	//�߳��б�
	for (int i = 0; i < initThreadSzie_; i++)
	{
		
		threads_[i]->start_();//ִ��һ���̺߳���
		idelTheadsSize_++;//ÿ����һ���̣߳������߳�����+1��
	}
}

//�����̺߳�����ִ������	�̳߳ص������̴߳��������������������
void ThreadPool::threadFunc(int threadid)
{
	//std::cout << "begin thradFunc()" << std::this_thread::get_id() << std::endl;
	//std::cout << "end thradFunc()" << std::this_thread::get_id() << std::endl;
	//һֱѭ��ȥ�����������ȡ����
	while (isPoolRunning_)

	{//����ʱ���߳̿����Ѿ�����while�����е��˴�
		auto lastTime = std::chrono::high_resolution_clock().now();//��ʼʱ��
		std::shared_ptr<Task> task;//
		{
			//��ȡ��
			std::unique_lock<std::mutex>lock(taskQueMtx_);//�������������ͷ���
			std::cout << "���Ի�ȡ����..." << std::this_thread::get_id() << std::endl;
			

			//��cachedģʽ�£��п����Ѿ������˺ܶ���̵߳��ǿ���ʱ�䳬��60s�Ļ�Ӧ�ý���������߳̽��л���
			//����initThreadSize_�������߳�Ҫ���л���
			// ��ǰʱ��-��һ���߳�ִ��ʱ��>60s,�����߳�
			
				//ÿһ�뷵��һ�Σ���ô���ֳ�ʱ���أ������������ִ�з���
			///����˫���ж�
				while (isPoolRunning_&&taskQue_.size()==0)//�������û���񣬽���while�ȴ�������������
				{
					//����������ʱ���أ�
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
								//��ʼ���յ�ǰ�߳�
								//��¼�߳�������ֵ�����޸�
								//���̶߳��󣬴��߳��б�������ɾ��,û�а취ƥ��treadFunc�̶߳����vector������
								// thread�̶߳���
								//�߳�id-���̶߳���ɾ��
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
						//�ȴ�notEmpty����
						noEmpty_.wait(lock);
					}
					//�̳߳�Ҫ�����������߳���Դ
					//if (!isPoolRunning_)
					//{
					//	threads_.erase(threadid);
					//	std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
					//	exitCond_.notify_all();//�������߳�
					//	return;//�����̺߳�����������ǰ�߳�
					//}
				}
				//�̳߳�Ҫ�����������߳���Դ
				//�̳߳�Ҫ�����������߳���Դ
				if (!isPoolRunning_)
				{
					//threads_.erase(threadid);
					//std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
					//exitCond_.notify_all();//�������߳�
					//return;//�����̺߳�����������ǰ�߳�
					break;
				}
		
			idelTheadsSize_--;//��ȡ����

			std::cout << "��ȡ����ɹ�" << std::this_thread::get_id() << std::endl;
			//�����������ȡһ���������
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;

			//�����Ȼ��ʣ���������֪ͨ�������߳�ִ������
			if (taskQue_.size() > 0)
			{
				noEmpty_.notify_all();
			}

			//ȡ��һ���������֪ͨ��֪ͨ���Լ����ύ��������
			notFull_.notify_all();
			//ȡ�������ͷ������������̼߳���ȡ����
		}
		if (task != nullptr)
		{
			//��ǰ�̸߳���ִ���������
			//task->run();//ִ�����񣻰�����ķ���ֵͨ��setVal��������Result��
			task->exec();
		}
		
		idelTheadsSize_++;//���������
		lastTime = std::chrono::high_resolution_clock().now();//�����̵߳���ִ���������ʱ��
	}

	threads_.erase(threadid);
	std::cout << "threadid:" << std::this_thread::get_id() << "exit()" << std::endl;
	exitCond_.notify_all();//����ִ��������߳�
}
// Task()����ʵ��
Task::Task()
	:result_(nullptr)
{}
void Task::exec()
{
	if (result_ != nullptr)
	{
		result_->setVal(run());//�������run����������̬
	}
	
}

void Task::setResult(Result* res)
{
	this->result_ = res;
}
///////�̷߳���ʵ��

int Thread::generateId_ = 0;

//�̹߳���
Thread::Thread(ThreadFunc func)
	:func_(func)
	,threadId_(generateId_++)
{

}

//�߳�����
Thread::~Thread()
{

}

//�����߳�
void Thread::start_()
{
	//����һ���߳���,ִ���̺߳���
	std::thread t(func_,threadId_);//
	t.detach();//����Ϊ�����߳�
}
//��ȡ�߳�id
int Thread::getId()const
{
	return threadId_;
}

/////Result������ʵ��
Result::Result(std::shared_ptr<Task> task,bool isValid)
	:isValid_(isValid)
	,task_(task)
{
	task_->setResult(this);
}

//get�������û��������������ȡtask�ķ���ֵ
Any Result::get()//�û�����
{
	if (!isValid_)
	{
		return "";
	}
	sem_.wait();//task�������û��ִ���꣬����������û��߳�
	return std::move(any_);
}
////setVal��������ȡ����ִ�з���ֵ�Ķ���

void Result::setVal(Any any)
{
	//�洢task�ķ���ֵ
	this->any_ = std::move(any);
	sem_.post(); //�Ѿ���ȡ������ķ���ֵ�������ź�����Դ
}