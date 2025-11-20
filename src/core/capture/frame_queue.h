#pragma once

#include <opencv2/core.hpp>
#include <optional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <cstddef>

namespace toriyomi {

/**
 * @brief Thread-safe frame queue for producer-consumer pattern
 * 
 * This queue is used to pass frames from the capture thread to the OCR thread.
 * When the queue is full, the oldest frame is automatically dropped.
 */
class FrameQueue {
public:
	/**
	 * @brief Construct a new Frame Queue
	 * @param maxSize Maximum number of frames to store. Default is 5.
	 */
	explicit FrameQueue(size_t maxSize = 5);
	
	/**
	 * @brief Destroy the Frame Queue
	 */
	~FrameQueue() = default;
	
	// Delete copy and move operations
	FrameQueue(const FrameQueue&) = delete;
	FrameQueue& operator=(const FrameQueue&) = delete;
	FrameQueue(FrameQueue&&) = delete;
	FrameQueue& operator=(FrameQueue&&) = delete;
	
	/**
	 * @brief Push a frame into the queue
	 * 
	 * If the queue is full, the oldest frame will be dropped. Frames are passed
	 * by value so callers can std::move them to avoid extra reference bumps.
	 *
	 * @param frame The frame to enqueue (copy or move)
	 */
	void Push(cv::Mat frame);
	
	/**
	 * @brief Pop a frame from the queue
	 * 
	 * This method blocks until a frame is available or timeout occurs. Frames
	 * are returned by move to avoid redundant clones.
	 * 
	 * @param timeoutMs Timeout in milliseconds
	 * @return std::optional<cv::Mat> The frame if available, std::nullopt if timeout
	 */
	std::optional<cv::Mat> Pop(int timeoutMs);
	
	/**
	 * @brief Get the current size of the queue
	 * @return size_t Number of frames in the queue
	 */
	size_t Size() const;
	
	/**
	 * @brief Clear all frames from the queue
	 */
	void Clear();

private:
	std::queue<cv::Mat> queue_;
	size_t maxSize_;
	mutable std::mutex mutex_;
	std::condition_variable condVar_;
};

}  // namespace toriyomi
