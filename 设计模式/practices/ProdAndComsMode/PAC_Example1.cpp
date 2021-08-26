#include <cstdlib>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <iostream>

static const int kItemRepositorySize = 3;   //队列大小
static const int kItemsToProduce = 10;      //待生产的数量
std::mutex mutex;

struct ItemRepository
{
    int item_buffer[kItemRepositorySize];   //缓冲区
    size_t read_position;                   //消费者读取产品位置
    size_t write_position;                  //生产者写入产品位置
    std::mutex mtx;                         //锁  
    std::condition_variable repo_not_full;  //条件变量，指示产品缓冲区不为满
    std::condition_variable repo_not_empty; //条件变量，指示产品缓冲区不为空 
}gItemRepository;

typedef struct ItemRepository ItemRepository;

void ProduceItem(ItemRepository* ir, int item)
{
    std::unique_lock<std::mutex> lock(ir->mtx);
    while(((ir->write_position + 1) % kItemRepositorySize) == ir->read_position)
    {
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
    while(ir->write_position == ir->read_position)
    {
	{
        std::lock_guard<std::mutex> lock(mutex);
	std::cout << "Buffer empty, wait for producer\n";
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

void ProducerTask()   //生产者任务
{
    for(int i = 1; i < kItemsToProduce; ++i)
    {
        ProduceItem(&gItemRepository, i);   //循环生产kItemToProduce个产品

	std::lock_guard<std::mutex> lock(mutex);
	std::cout << "produce " << i << "th product" << std::endl;
    }
}

void ConsumerTask()  //消费者任务
{
    static int cnt = 0;
    while(1)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
	int item = ConsumeItem(&gItemRepository);   //消费一个产品

	std::lock_guard<std::mutex> lock(mutex);
	std::cout << "consume " << item << "th product" << std::endl;
	
        if(++cnt == kItemsToProduce) break;
    }

}

void InitItemRepository(ItemRepository *ir)   //初始化写入和读取位置
{
    ir->write_position = 0;
    ir->read_position = 0;
}

int main()
{
    InitItemRepository(&gItemRepository);
    std::thread producer(ProducerTask);   //创建生产者线程
    std::thread consumer(ConsumerTask);   //创建消费者线程

    producer.join();
    consumer.join();

    return 0;
}
