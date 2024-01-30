#include "ReceivedFrame.hpp"
class FrameBuffer {
public:
	ReceivedFrame next() {
		std::unique_lock<std::mutex> guard(m);
		ReceivedFrame next_frame = frame_queue.top();
		frame_queue.pop();
		current_framenr = next_frame.get_framenr();
		guard.unlock();
		return std::move(next_frame);
	}
	bool insert_frame(ReceivedFrame& frame) {
		// Due to UDP out of order delivery it might be possible that a complete frame is received too late
		// i.e. after the next frame was already requested
		std::unique_lock<std::mutex> guard(m);
		if (frame.get_framenr() < current_framenr) {
			return false;
		}
		frame_queue.push(std::move(frame));
		guard.unlock();
		return true;
	}
	size_t get_buffer_size() {
		return frame_queue.size();
	}

	uint32_t get_current_framenr() {
		return current_framenr;
	}
private:
	uint32_t current_framenr;
	std::priority_queue<ReceivedFrame> frame_queue;
	std::mutex m;
};