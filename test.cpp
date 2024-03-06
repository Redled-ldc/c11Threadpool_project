#include "threadpool.h"
#include <iostream>
#include <chrono>
/*
	��Щ����ϣ����ȡ�̷߳��ص��̳߳�
	������
	1+++ 30000�ĺ�
	thread1 1+10000
	thread2 10001��������20000
	thread3 20001��������30000
	���̣߳���ÿһ���̷߳������ռ䣬���ҵ����Ǽ��������أ��ϲ����յĽ������
*/
using Ulong = unsigned long long;
class MyTask :public Task
{
public:
	MyTask(int  begin, int  end)
		:begin_(begin),end_(end)
	{

	}
	//������run�����ķ���ֵ
	Any run()
	{
		std::cout << "begin run()" << std::this_thread::get_id() << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(3));
		Ulong sum = 0;
		for (Ulong i = begin_; i <= end_; i++)
		{
			sum += i;
		}
		std::cout << "end run()" << std::this_thread::get_id() << std::endl;
		return sum;
	}
private:
	int begin_;
	int  end_;
};
int main()
{

	
	{//ThreadPool pool������������λ����߳������Դ

		ThreadPool pool;
		//�û��Լ������̳߳صĹ���ģʽ
		pool.setMode(PoolMode::MODE_CACHED);
		//�����̳߳�
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		//����������result�Ľ��
		//pool.submitTask(std::make_shared<Mytask>());
		//pool.submitTask(std::make_shared<Mytask>());
		//pool.submitTask(std::make_shared<Mytask>());
		//pool.submitTask(std::make_shared<Mytask>());
		Ulong sum1 = res1.get().cast_<Ulong>();
		Ulong sum2 = res2.get().cast_<Ulong>();
		Ulong sum3 = res3.get().cast_<Ulong>();
		std::cout << "result1=" << (sum1 + sum2 + sum3) << std::endl;
		Ulong sum4 = 0;
		for (Ulong i = 1; i <= 300000000; i++)
		{
			sum4 += i;
		}
		std::cout << "result2=" << sum4 << std::endl;
		
	}//�뿪������result�ֲ������ͷţ�������������
	//��vs�£����������������ͷ���Դ
	int a = getchar();

	return 0;
}