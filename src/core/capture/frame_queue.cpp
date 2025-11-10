#include "frame_queue.h"
#include <chrono>

namespace toriyomi {

FrameQueue::FrameQueue(size_t maxSize)
	: maxSize_(maxSize) {
	if (maxSize_ == 0) {
		maxSize_ = 1;  // Minimum size is 1
	}
}

void FrameQueue::Push(const cv::Mat& frame) {
	std::lock_guard<std::mutex> lock(mutex_);
	
	// If queue is full, drop the oldest frame
	if (queue_.size() >= maxSize_) {
		queue_.pop();
	}
	
	// Clone the frame to ensure deep copy
	queue_.push(frame.clone());
	
	// Notify waiting consumers
	condVar_.notify_one();
}

std::optional<cv::Mat> FrameQueue::Pop(int timeoutMs) {
	std::unique_lock<std::mutex> lock(mutex_);
	
	// Wait for a frame or timeout
	bool hasFrame = condVar_.wait_for(
		lock,
		std::chrono::milliseconds(timeoutMs),
		[this] { return !queue_.empty(); }
	);
	
	// If timeout or no frame, return nullopt
	if (!hasFrame || queue_.empty()) {
		return std::nullopt;
	}
	
	// Get the frame
	cv::Mat frame = queue_.front();
	queue_.pop();
	
	return frame;
}

size_t FrameQueue::Size() const {
	std::lock_guard<std::mutex> lock(mutex_);
	return queue_.size();
}

void FrameQueue::Clear() {
	std::lock_guard<std::mutex> lock(mutex_);
	
	// Clear the queue by creating a new empty queue
	std::queue<cv::Mat> empty;
	std::swap(queue_, empty);
}

}  // namespace toriyomi
