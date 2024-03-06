#include "threadpool.h"
#include <iostream>
#include <chrono>
/*
	有些场景希望获取线程返回的线程池
	举例：
	1+++ 30000的和
	thread1 1+10000
	thread2 10001————20000
	thread3 20001————30000
	主线程：给每一个线程分配计算空间，并且等他们计算结果返回，合并最终的结果即可
*/
using Ulong = unsigned long long;
class MyTask :public Task
{
public:
	MyTask(int  begin, int  end)
		:begin_(begin),end_(end)
	{

	}
	//如何设计run（）的返回值
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

	
	{//ThreadPool pool对象析构后如何回收线程相关资源

		ThreadPool pool;
		//用户自己设置线程池的工作模式
		pool.setMode(PoolMode::MODE_CACHED);
		//启动线程池
		pool.start(4);
		Result res1 = pool.submitTask(std::make_shared<MyTask>(1, 100000000));
		Result res2 = pool.submitTask(std::make_shared<MyTask>(100000001, 200000000));
		Result res3 = pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));

		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		pool.submitTask(std::make_shared<MyTask>(200000001, 300000000));
		//如何设计这里result的结果
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
		
	}//离开作用域，result局部对象释放，调用析构函数
	//在vs下，条件变量析构会释放资源
	int a = getchar();

	return 0;
}