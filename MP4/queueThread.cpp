#include <vector>
#include <mutex>
#include "utils.h"

using namespace std;
mutex vectorMutex;
vector<int> transferId;

int communicator(int transferNum) {
	if (!transferNum)
		return 0;
	int ret = 0;
	vectorMutex.lock();
	if (!balanceId.empty()) {
		string msg;
		if (transferNum <= balanceId.size()) {
			for (int i = balanceId.size()-1; i >= balanceId.size()-transferNum; i--) {
				msg += to_string(balanceId[i]) + " ";
				balanceId.pop_back();
			}
			write_all_to_socket(msg, msg.length());
		} else {
			ret = transferNum - balanceId.size();
			for (int i = balanceId.size()-1; i >= 0; i--)
				msg += to_string(balanceId[i]) + " ";
			transferId.clear();
		}
	}	
	vectorMutex.unlock();
	return ret;
}
