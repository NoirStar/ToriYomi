#include <gtest/gtest.h>
#include <opencv2/core.hpp>
#include <thread>
#include <chrono>
#include "core/capture/frame_queue.h"

using namespace toriyomi;
using namespace std::chrono_literals;

// Test fixture for FrameQueue
class FrameQueueTest : public ::testing::Test {
protected:
	void SetUp() override {
		queue = std::make_unique<FrameQueue>(5);  // Max size of 5
	}

	void TearDown() override {
		queue.reset();
	}

	std::unique_ptr<FrameQueue> queue;
};

// Test 1: Push and Pop single frame
TEST_F(FrameQueueTest, PushAndPopSingleFrame) {
	// Create a test frame (100x100, BGR)
	cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));
	
	queue->Push(frame);
	
	auto poppedFrame = queue->Pop(1000);  // 1 second timeout
	
	ASSERT_TRUE(poppedFrame.has_value());
	EXPECT_EQ(poppedFrame->rows, 100);
	EXPECT_EQ(poppedFrame->cols, 100);
	EXPECT_EQ(poppedFrame->type(), CV_8UC3);
}

// Test 2: Pop from empty queue with timeout
TEST_F(FrameQueueTest, PopFromEmptyQueueTimeout) {
	auto start = std::chrono::steady_clock::now();
	
	auto result = queue->Pop(100);  // 100ms timeout
	
	auto end = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	
	EXPECT_FALSE(result.has_value());
	EXPECT_GE(elapsed, 100);  // Should wait at least 100ms
	EXPECT_LT(elapsed, 200);  // Should not wait too long
}

// Test 3: FIFO order (First In First Out)
TEST_F(FrameQueueTest, FIFOOrder) {
	// Push 3 frames with different colors
	cv::Mat frame1(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));    // Blue
	cv::Mat frame2(100, 100, CV_8UC3, cv::Scalar(0, 255, 0));    // Green
	cv::Mat frame3(100, 100, CV_8UC3, cv::Scalar(0, 0, 255));    // Red
	
	queue->Push(frame1);
	queue->Push(frame2);
	queue->Push(frame3);
	
	// Pop and check order
	auto pop1 = queue->Pop(1000);
	auto pop2 = queue->Pop(1000);
	auto pop3 = queue->Pop(1000);
	
	ASSERT_TRUE(pop1.has_value());
	ASSERT_TRUE(pop2.has_value());
	ASSERT_TRUE(pop3.has_value());
	
	// Check colors (Blue, Green, Red)
	EXPECT_EQ(pop1->at<cv::Vec3b>(0, 0), cv::Vec3b(255, 0, 0));
	EXPECT_EQ(pop2->at<cv::Vec3b>(0, 0), cv::Vec3b(0, 255, 0));
	EXPECT_EQ(pop3->at<cv::Vec3b>(0, 0), cv::Vec3b(0, 0, 255));
}

// Test 4: Overflow - old frames should be dropped
TEST_F(FrameQueueTest, OverflowDropsOldestFrame) {
	// Push 6 frames (max size is 5)
	for (int i = 0; i < 6; i++) {
		cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(i, i, i));
		queue->Push(frame);
	}
	
	// Queue size should be 5
	EXPECT_EQ(queue->Size(), 5);
	
	// First frame should be frame 1 (frame 0 was dropped)
	auto firstFrame = queue->Pop(1000);
	ASSERT_TRUE(firstFrame.has_value());
	EXPECT_EQ(firstFrame->at<cv::Vec3b>(0, 0), cv::Vec3b(1, 1, 1));
}

// Test 5: Thread safety - concurrent push and pop
TEST_F(FrameQueueTest, ThreadSafety) {
	const int numFrames = 100;
	
	// Producer thread
	std::thread producer([this, numFrames]() {
		for (int i = 0; i < numFrames; i++) {
			cv::Mat frame(50, 50, CV_8UC3, cv::Scalar(i, i, i));
			queue->Push(frame);
			std::this_thread::sleep_for(1ms);
		}
	});
	
	// Consumer thread
	int consumedCount = 0;
	std::thread consumer([this, &consumedCount, numFrames]() {
		while (consumedCount < numFrames) {
			auto frame = queue->Pop(100);
			if (frame.has_value()) {
				consumedCount++;
			}
		}
	});
	
	producer.join();
	consumer.join();
	
	EXPECT_EQ(consumedCount, numFrames);
}

// Test 6: Size() returns correct count
TEST_F(FrameQueueTest, SizeReturnsCorrectCount) {
	EXPECT_EQ(queue->Size(), 0);
	
	cv::Mat frame(100, 100, CV_8UC3);
	
	queue->Push(frame);
	EXPECT_EQ(queue->Size(), 1);
	
	queue->Push(frame);
	queue->Push(frame);
	EXPECT_EQ(queue->Size(), 3);
	
	queue->Pop(1000);
	EXPECT_EQ(queue->Size(), 2);
	
	queue->Pop(1000);
	queue->Pop(1000);
	EXPECT_EQ(queue->Size(), 0);
}

// Test 7: Clear method empties the queue
TEST_F(FrameQueueTest, ClearEmptiesQueue) {
	cv::Mat frame(100, 100, CV_8UC3);
	
	queue->Push(frame);
	queue->Push(frame);
	queue->Push(frame);
	
	EXPECT_EQ(queue->Size(), 3);
	
	queue->Clear();
	
	EXPECT_EQ(queue->Size(), 0);
	
	// Pop should timeout
	auto result = queue->Pop(50);
	EXPECT_FALSE(result.has_value());
}

// Test 8: Multiple consecutive pops on empty queue
TEST_F(FrameQueueTest, MultipleConsecutivePopsOnEmpty) {
	for (int i = 0; i < 5; i++) {
		auto result = queue->Pop(10);
		EXPECT_FALSE(result.has_value());
	}
}
