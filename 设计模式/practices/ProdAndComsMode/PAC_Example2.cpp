#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>

static const int kItemRepositorySize = 4;   //队列大小
static const int kItemsToProduce = 10;      //待生产的数量
std::mutex mutex;

struct ItemRepository
{
    int item_buffer[kItemRepositorySize];   //缓冲区
    size_t read_position;                   //消费者读取产品位置
    size_t write_position;                  //生产者写入产品位置
    size_t item_counter;                    //产品个数
    std::mutex mtx;                         //锁  
    std::mutex item_counter_mtx;            //锁
    std::condition_variable repo_not_full;  //条件变量，指示产品缓冲区不为满
    std::condition_variable repo_not_empty; //条件变量，指示产品缓冲区不为空 
}gItemRepository;

typedef struct ItemRepository ItemRepository;

void ProduceItem(ItemRepository* ir, int item)
{
    std::unique_lock<std::mutex> lock(ir->mtx);
    while(((ir->write_position + 1) % kItemRepositorySize) == ir->read_position)
    {
	//item buffer is full, just wait here
        {
            std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "Buffer full, wait for it\n";
	}
	(ir->repo_not_full).wait(lock);
    }

    (ir->item_buffer)[ir->write_position] = item;    //写入产品
    (ir->write_position)++;

    if(ir->write_position == kItemRepositorySize)
    {
       ir->write_position = 0;   //写入位置若是在队列最后则重新设置为初始位置
    }

    (ir->repo_not_empty).notify_all();    //通知消费者
    lock.unlock();    //解锁
}

int ConsumeItem(ItemRepository* ir)
{
    int data;
    std::unique_lock<std::mutex> lock(ir->mtx);
    // item buffer is empty, just wait here.
    while(ir->write_position == ir->read_position)
    {
	{
        std::lock_guard<std::mutex> lock(mutex);
	std::cout << "Buffer empty, wait for producer------------------------------------\n";
	}

	(ir->repo_not_empty).wait(lock);
    }

    data = (ir->item_buffer)[ir->read_position];   //读取某一产品
    (ir->read_position)++;

    if(ir->read_position >= kItemRepositorySize)
    {
        ir->read_position = 0;
    }
    
    (ir->repo_not_full).notify_all();   //通知消费者
    lock.unlock();   //解锁

    return data;    //返回产品
}

void ProducerTask()
{
    for(int i = 1; i <= kItemsToProduce; ++i)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(6));
	ProduceItem(&gItemRepository, i);
	{
            std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "producer" << std::this_thread::get_id() << " produce " << i << "th product" << std::endl;
	}
    }
    {
	std::lock_guard<std::mutex> lock(mutex);
	std::cout << "producer" << std::this_thread::get_id() << " quit---------------" << std::endl;
    }
}

void ProducerTask1()   //生产者任务
{
    int i = 1;
    while(1)
    {
	std::this_thread::sleep_for(std::chrono::milliseconds(6));
        ProduceItem(&gItemRepository, i);   //循环生产kItemToProduce个产品

	{
	    std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "producer " << std::this_thread::get_id() << " produce " << i << "th product" << std::endl;
	}
	i++;
	if(i >= kItemsToProduce)
	{
	   i = 1;
	}
        {
	    std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "producer" << std::this_thread::get_id() << " quit------------" << std::endl;
        }
    }
}

void ConsumerTask()  //消费者任务
{
    bool ready_to_exit = false;
    while(1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
	std::unique_lock<std::mutex> lock(gItemRepository.item_counter_mtx);
	if (gItemRepository.item_counter < kItemsToProduce)
	{
	    int item = ConsumeItem(&gItemRepository);   //消费一个产品
	    ++(gItemRepository.item_counter);
	    {
		std::lock_guard<std::mutex> lock(mutex);
		std::cout << "consumer" <<std::this_thread::get_id() << " consume " << item << "th product." << std::endl;
	    }
	}else
	{
            ready_to_exit = true;
	}
	lock.unlock();

	if(ready_to_exit == true)
	{
            break;
	}
    }
	{
	    std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "consumer" << std::this_thread::get_id() << " quit===========" << std::endl;
	}

}

void ConsumerTask1()
{
    while(1)
    {
	std::this_thread::sleep_for(std::chrono::seconds(1));
	std::unique_lock<std::mutex> lock(gItemRepository.item_counter_mtx);
	
	int item = ConsumeItem(&gItemRepository);
	++(gItemRepository.item_counter);
	{
            std::lock_guard<std::mutex> lock(mutex);
	    std::cout << "consumer" << std::this_thread::get_id() << " consume " << item << "th product." << std::endl;
	}
	lock.unlock();
    }
    {
        std::lock_guard<std::mutex> lock(mutex);
	std::cout << "consumer" << std::this_thread::get_id() << " quit." << std::endl;
    }
}

void InitItemRepository(ItemRepository *ir)   //初始化写入和读取位置
{
    ir->write_position = 0;
    ir->read_position = 0;
    ir->item_counter = 0;
}

void test()
{
    InitItemRepository(&gItemRepository);
    std::thread producer(ProducerTask);
    std::thread consumer1(ConsumerTask);
    std::thread consumer2(ConsumerTask);
    std::thread consumer3(ConsumerTask);
    std::thread consumer4(ConsumerTask);

    producer.join();
    consumer1.join();
    consumer2.join();
    consumer3.join();
    consumer4.join();
}

int main()
{
    test();
    return 0;
}
