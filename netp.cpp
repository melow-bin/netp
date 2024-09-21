// netp 0.0.1.
#include <iostream>
#include <string>
#include <array>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <format>
#include <memory>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>

// The Netface class provides methods to query various properties of a network interface.
// It allows retrieving information such as IP address, MAC address, subnet mask, broadcast address,
// and whether the interface is in promiscuous mode.
class Netface {
private:
	std::string ifname;
	int sockfd;
	struct ifreq ifr;
	// Checks if socket valid
	void check_socket() const;
	//Performs ioctl operation with specified request. Throws an exception if the socket is not initialized properly.
	void ioctl_request(int request);
public:
	explicit Netface(const std::string& ifname); // Initializes a socket for interacting with the specified network interface.
	~Netface();

	std::string get_ip_address(); // Returns the IP address assigned to the network interface.
	std::string get_mac_address(); // Returns the MAC address of the network interface.
	std::string get_subnet_mask(); // Returns the subnet mask of the network interface.
	std::string get_broadcast_address(); // Returns the broadcast address of the network interface.
	bool is_promiscuous_mode(); // Returns true if the interface is in promiscuous mode, false otherwise.
	bool ad_is_promiscuous_mode(); 
	// Checks dmesg logs if the network interface is in promiscuous mode
	// 'Cuz i still didn't get why ioctl request doesnt retrives fucking 
	// promiscuous mode when promiscuous mode actullay open by tcpdump or tshark/wireshark (pls help me)
};

// Helper method to ensure that the socket is valid
void Netface::check_socket() const {
	if (sockfd < 0) {
		throw std::runtime_error("socket not initialized properly.");
	}
}
// Helper method to perform ioctl operations with error handling
void Netface::ioctl_request(int request) {
	check_socket();
	if (ioctl(sockfd, request, &ifr) < 0) {
		throw std::runtime_error("ioctl error: " + std::string(strerror(errno)));
	}
}
// Initializes a socket for interacting with the specified network interface.
Netface::Netface(const std::string& ifname)
	: ifname(ifname), sockfd(socket(AF_INET, SOCK_DGRAM, 0)) {
		if (sockfd < 0) {
			throw std::runtime_error("socket creation error: " + std::string(strerror(errno)));
		}
			std::strncpy(ifr.ifr_name, ifname.c_str(), IFNAMSIZ - 1);
			ifr.ifr_name[IFNAMSIZ - 1] = '\0';
		}
Netface::~Netface() {
	if (sockfd >= 0) {
		close(sockfd);
	}
}
// Retrieves the IPv4 address assigned tof the network interface
std::string Netface::get_ip_address() {
	ifr.ifr_addr.sa_family = AF_INET;
	ioctl_request(SIOCGIFADDR);
	return inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);
}
// Retrieves the MAC address of the network interface
std::string Netface::get_mac_address() {
	ioctl_request(SIOCGIFHWADDR);
	auto* mac = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
	return std::format("{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
// Retrieves the subnet mask of the network interface
std::string Netface::get_subnet_mask() {
	ifr.ifr_addr.sa_family = AF_INET;
	ioctl_request(SIOCGIFNETMASK);
	return inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);   
}
// Retrieves the broadcast address of the network interface
std::string Netface::get_broadcast_address() {
	ifr.ifr_addr.sa_family = AF_INET;
	ioctl_request(SIOCGIFBRDADDR);
	return inet_ntoa(((struct sockaddr_in*)&ifr.ifr_addr)->sin_addr);   
}
// Checks if the network interface is in promiscuous mode
bool Netface::is_promiscuous_mode() {
	ioctl_request(SIOCGIFFLAGS);
	return ifr.ifr_flags & IFF_PROMISC;
}
// Checks dmesg logs if the network interface is in promiscuous mode
bool Netface::ad_is_promiscuous_mode() {
	const std::string command = "dmesg | grep -E '" + ifname + ".*promiscuous mode'";
	std::array<char, 256> buffer;
	std::string result;

	std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
	if (!pipe) {
		throw std::runtime_error("popen error: " + std::string(strerror(errno)));
		return false;
	}
	// Read the output from the pipe into a buffer and appends it to a string
	while(fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result.append(buffer.data());
	}
	// Process the outpıt
	bool promiscuous_mode = false;
	std::istringstream result_stream(result);
	std::string line;
	while (std::getline(result_stream, line)) {
		if (line.find("entered promiscuous mode") != std::string::npos) {
			promiscuous_mode = true;
		} else if (line.find("left promiscuous mode") != std::string::npos) {
			promiscuous_mode = false;
		}
	}
	return promiscuous_mode;
}
// Retrieves a list of network interface names on the system
std::vector<std::string> get_network_interfaces() {
	std::vector<std::string> interfaces;
	struct ifaddrs *addrs, *tmp;
	// Get the list of network interfaces
	if (getifaddrs(&addrs) == -1) {
		throw std::runtime_error("getifaddrs error: " + std::string(strerror(errno)));
	}
	for (tmp = addrs; tmp != nullptr; tmp = tmp->ifa_next) {
		if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET) {
			interfaces.push_back(tmp->ifa_name);
		}
	}
	freeifaddrs(addrs);
	return interfaces;
}

int main() {
	try {
		auto interfaces = get_network_interfaces();
		if (interfaces.empty()) {
			std::cout << "No network interfaces found." << std::endl;
			return EXIT_SUCCESS;
		}
		std::cout << "---NETP---";
		for (const auto& iface_name : interfaces) {
			try {
				Netface iface(iface_name);
				std::cout << "\n>> Interface: " << iface_name << std::endl;
				std::cout << "\tIP address: " << iface.get_ip_address() << " (IPv4 assigned)\n";
				std::cout << "\tMAC address: " << iface.get_mac_address() << " (Hardware address)\n";
				std::cout << "\tBroadcast address: " << iface.get_broadcast_address() << "\n";
				std::cout << "\tSubnet mask: " << iface.get_subnet_mask() << "\n";
				std::cout << "\tPromiscuous mode: " << (iface.is_promiscuous_mode() ? "Enabled" : "Disabled") << "\n";
				std::cout << "\t(LOG) Promiscuous mode: " << (iface.ad_is_promiscuous_mode() ? "Enabled" : "Disabled") << "\n";
			} catch (const std::exception& e) {
				std::cerr << "Error with interface " << iface_name << ": " << e.what() << std::endl;
			}
		}
	} catch (const std::exception& e) {
		std::cerr << "Error with get network interfaces " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
