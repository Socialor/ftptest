//含有NAT穿透功能的客户端程序
#pragma GCC optimize(2)
#include <iostream>
#include <iomanip>
#include <dirent.h>
#include <cstdlib>
#include <termios.h>
#include <unistd.h>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <sys/stat.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <sys/time.h>
#include <stdio.h>

using namespace std;

#define BUFSIZE 1024
#define NATPORT 20324

const string NAT_IP = "121.4.224.98";

class client
{
	int con_fd, offset;
	string eth, ser_ip;
	sockaddr_in ser_addr;
	char buf[BUFSIZE + 1];
	bool connected, debug_mode, bi_mode, pasv_mode, interactive_mode;

	int rm_r(char* buf)
	{
		int p, q;
		p = q = 0;
		while (buf[q])
		{
			if (buf[q] != '\r' || buf[q + 1] != '\n')
			{
				buf[p] = buf[q];
				++p;
			}
			++q;
		}
		buf[p] = 0;
		return p;
	}

	void ftp_connect(const char* ip)
	{
		con_fd = socket(AF_INET, SOCK_STREAM, 0);
		while (con_fd == -1)
		{
			con_fd = socket(AF_INET, SOCK_STREAM, 0);
		}
		memset(&ser_addr, 0, sizeof(sockaddr_in));
		inet_aton(NAT_IP.c_str(), &ser_addr.sin_addr);
		ser_addr.sin_family = AF_INET;
		ser_addr.sin_port = htons(NATPORT);
		if (connect(con_fd, (sockaddr*)&ser_addr, sizeof(sockaddr)) == -1)
		{
			cout << "ftp: connect: Connection refused\n";
			connected = false;
			close(con_fd);
		}
		else
		{
			string op = ip;
			op = "CONC " + op + "\r\n";
			while (send(con_fd, op.c_str(), op.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				cout << "421 Timeout.\n";
				cout << "ftp: connect: Connection refused\n";
				return;
			}
			buf[num] = 0;
			if (get_code(buf)==500)
			{
				connected = false;
				close(con_fd);
				cout << "ftp: connect: Connection refused\n";
				return;
			}
			string nat_pt = buf + 4;
			ser_addr.sin_port = htons(stoi(nat_pt));
			close(con_fd);
			con_fd = socket(AF_INET, SOCK_STREAM, 0);
			while (con_fd == -1)
			{
				con_fd = socket(AF_INET, SOCK_STREAM, 0);
			}
			if (connect(con_fd, (sockaddr*)&ser_addr, sizeof(sockaddr)) == -1)
			{
				cout << "ftp: connect: Connection refused\n";
				connected = false;
				close(con_fd);
			}
			timeval timeo = { 5, 0 };
			setsockopt(con_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
			setsockopt(con_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				cout << "ftp: connect: Connection refused\n";
				return;
			}
			buf[num] = 0;
			cout << "Connnected to " << ip << ".\n";
			connected = true;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				return;
			}
		}
	}

	string get_pw()
	{
		string res;
		char c;
		static struct termios oldt, newt;
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON);
		tcsetattr(STDIN_FILENO, TCSANOW, &newt);
		int r;
		r = system("stty -echo");
		c = getchar();
		while (c != '\r' && c != '\n')
		{
			if (c == 127)
			{
				if (res.size())
				{
					res.pop_back();
				}
			}
			else
			{
				res.push_back(c);
			}
			c = getchar();
		}
		r = system("stty echo");
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
		return res;
	}

	int get_code(char* msg)
	{
		if (!msg)
		{
			return -1;
		}
		return 100 * (msg[0] - '0') + 10 * (msg[1] - '0') + msg[2] - '0';
	}

	void login()
	{
		if (!connected)
		{
			return;
		}
		passwd* pwd;
		pwd = getpwuid(getuid());
		cout << "Name (" << inet_ntoa(ser_addr.sin_addr) << ":" << pwd->pw_name << "): ";
		string user, pw;
		getline(cin, user);
		user = "USER " + user + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << user;
		}
		while (send(con_fd, user.c_str(), user.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (get_code(buf) != 331)
		{
			cout << "Login failed.\n";
			return;
		}
		cout << "Password: ";
		pw = get_pw();
		cout << endl;
		pw = "PASS " + pw + "\r\n";
		if (debug_mode)
		{
			cout << "---> PASS XXXX\n";
		}
		while (send(con_fd, pw.c_str(), pw.size(), 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (get_code(buf) != 230)
		{
			cout << "Login failed.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> SYST\n";
		}
		while (send(con_fd, "SYST\r\n", 6, 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> TYPE I\n";
		}
		while (send(con_fd, "TYPE I\r\n", 8, 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		bi_mode = true;
	}

	vector<string> split(string& s, string& d) //字符串分割函数
	{
		if (s == "")
		{
			return {};
		}
		char* ss = new char[s.size() + 1];
		strcpy(ss, s.c_str());
		char* dd = new char[d.size() + 1];
		strcpy(dd, d.c_str());
		char* p = strtok(ss, dd);
		vector<string> res;
		while (p)
		{
			res.push_back(p);
			p = strtok(NULL, dd);
		}
		delete[]ss;
		delete[]dd;
		return res;
	}

	int vector_find(vector<string>& l, string& s)
	{
		int i;
		for (i = 0; i < l.size(); ++i)
		{
			if (l[i] == s)
			{
				break;
			}
		}
		return i;
	}

	bool is_num(string& s)
	{
		for (auto& ch : s)
		{
			if (ch < '0' || ch>'9')
			{
				return false;
			}
		}
		return true;
	}

	void ftp_debug(bool __debug_mode)
	{
		debug_mode = __debug_mode;
		if (debug_mode)
		{
			cout << "Debugging on.\n";
		}
		else
		{
			cout << "Debugging off.\n";
		}
	}

	void ftp_ascii()
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> TYPE A\n";
		}
		while (send(con_fd, "TYPE A\r\n", 8, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		bi_mode = false;
	}

	void ftp_bi()
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> TYPE I\n";
		}
		while (send(con_fd, "TYPE I\r\n", 8, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		bi_mode = true;
	}

	void ftp_close()
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> QUIT\n";
		}
		while (send(con_fd, "QUIT\r\n", 6, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			return;
		}
		buf[num] = 0;
		cout << buf;
		connected = false;
		close(con_fd);
	}

	void ftp_status()
	{
		if (connected)
		{
			cout << "Connected to " << ser_ip << ".\n";
		}
		else
		{
			cout << "Not connected.\n";
		}
		cout << "Type: " << ((bi_mode) ? "binary" : "ascii") << "; ";
		cout << "Debugging: " << ((debug_mode) ? "on" : "off") << "; \n";
		cout << "Passive mode: " << ((pasv_mode) ? "on" : "off") << "; \n";
		cout << "Interactive mode: " << ((interactive_mode) ? "on" : "off") << "; \n";
	}

	void ftp_lcd(string& local_path)
	{
		if (chdir(local_path.c_str()) == -1)
		{
			cout << "local: " << local_path << ": No such directory\n";
		}
		else
		{
			char path[255];
			cout << "Local directory now " << getcwd(path, 255) << endl;
		}
	}

	void ftp_pasv()
	{
		pasv_mode = true;
		cout << "NAT traversal service doesn't support active mode.\n";
		cout << "Passive mode: " << ((pasv_mode) ? "on" : "off") << "; \n";
	}

	void ftp_open(const char* ip)
	{
		srand(time(0));
		ser_ip = ip;
		ftp_connect(ip);
		login();
	}

	void ftp_user(string& user, string pw = "", string ac = "")
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		user = "USER " + user + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << user;
		}
		while (send(con_fd, user.c_str(), user.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (get_code(buf) != 331)
		{
			cout << "Login failed.\n";
			return;
		}
		if (!pw.size())
		{
			cout << "Password: ";
			pw = get_pw();
			cout << endl;
		}
		pw = "PASS " + pw + "\r\n";
		if (debug_mode)
		{
			cout << "---> PASS XXXX\n";
		}
		while (send(con_fd, pw.c_str(), pw.size(), 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (get_code(buf) != 230)
		{
			cout << "Login failed.\n";
			return;
		}
		if (ac.size())
		{
			ac = "ACCT " + ac + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << ac;
			}
			while (send(con_fd, ac.c_str(), ac.size(), 0) == -1);
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				cout << "421 Timeout.\n";
				cout << "Login failed.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
		}
		if (debug_mode)
		{
			cout << "---> SYST\n";
		}
		while (send(con_fd, "SYST\r\n", 6, 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> TYPE I\n";
		}
		while (send(con_fd, "TYPE I\r\n", 8, 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			cout << "Login failed.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			cout << "Login failed.\n";
			return;
		}
		bi_mode = true;
	}

	void ftp_cd(string& remote_path)
	{
		remote_path = "CWD " + remote_path + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remote_path;
		}
		while (send(con_fd, remote_path.c_str(), remote_path.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
	}

	void ftp_pwd()
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> PWD\n";
		}
		while (send(con_fd, "PWD\r\n", 5, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_mkdir(string& dir)
	{
		dir = "MKD " + dir + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << dir;
		}
		while (send(con_fd, dir.c_str(), dir.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_rmdir(string& dir)
	{
		dir = "RMD " + dir + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << dir;
		}
		while (send(con_fd, dir.c_str(), dir.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_remotehelp(string cmd = "")
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (cmd.size())
		{
			cmd = "HELP " + cmd + "\r\n";
		}
		else
		{
			cmd = "HELP\r\n";
		}
		if (debug_mode)
		{
			cout << "---> " << cmd;
		}
		while (send(con_fd, cmd.c_str(), cmd.size(), 0) == -1);
		timeval timeo = { 0, 100000 };
		setsockopt(con_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
		int num;
		while ((num = recv(con_fd, buf, BUFSIZE, 0)) > 0)
		{
			buf[num] = 0;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				return;
			}
			cout << buf;
		}
		timeo.tv_sec = 5;
		timeo.tv_usec = 0;
		setsockopt(con_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
	}

	void ftp_rename(string& from, string& to)
	{
		from = "RNFR " + from + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << from;
		}
		while (send(con_fd, from.c_str(), from.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		if (get_code(buf) != 350)
		{
			return;
		}
		to = "RNTO " + to + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << to;
		}
		while (send(con_fd, to.c_str(), to.size(), 0) == -1);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_quote(string& cmd)
	{
		cmd += "\r\n";
		if (debug_mode)
		{
			cout << "---> " << cmd;
		}
		while (send(con_fd, cmd.c_str(), cmd.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_prompt()
	{
		interactive_mode = !interactive_mode;
		cout << "Interactive mode: " << ((interactive_mode) ? "on" : "off") << "; \n";
	}

	void ftp_delete(string& remote_file)
	{
		remote_file = "DELE " + remote_file + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remote_file;
		}
		while (send(con_fd, remote_file.c_str(), remote_file.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	int data_pasv()
	{
		if (debug_mode)
		{
			cout << "---> PASV\n";
		}
		while (send(con_fd, "PASV\r\n", 6, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return -1;
		}
		buf[num] = 0;
		cout << buf;
		string port = buf + 4;
		int data_fd = socket(AF_INET, SOCK_STREAM, 0);
		while (data_fd == -1)
		{
			data_fd = socket(AF_INET, SOCK_STREAM, 0);
		}
		timeval timeo = {0, 200000};
		setsockopt(data_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
		setsockopt(data_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
		sockaddr_in serdata = ser_addr;
		serdata.sin_port = htons(stoi(port));
		if (connect(data_fd, (sockaddr*)&serdata, sizeof(sockaddr)) == -1)
		{
			cout << "ftp: connect: Connection refused\n";
			connected = false;
			close(con_fd);
			close(data_fd);
			return -1;
		}
		return data_fd;
	}

	int get_local_ip(char* ip)
	{
		int sockfd;
		struct sockaddr_in sin;
		struct ifreq ifr;

		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (sockfd == -1)
		{
			printf("socket error: %s\n", strerror(errno));
			return -1;
		}
		strncpy(ifr.ifr_name, eth.c_str(), IFNAMSIZ);
		ifr.ifr_name[IFNAMSIZ - 1] = 0;
		if (ioctl(sockfd, SIOCGIFADDR, &ifr) < 0)
		{
			printf("ioctl error: %s\n", strerror(errno));
			close(sockfd);
			return -1;
		}
		memcpy(&sin, &ifr.ifr_addr, sizeof(sin));
		snprintf(ip, 16, "%s", inet_ntoa(sin.sin_addr));
		close(sockfd);
		return 0;
	}

	int data_actv()
	{
		int data_fd = socket(AF_INET, SOCK_STREAM, 0);
		while (data_fd == -1)
		{
			data_fd = socket(AF_INET, SOCK_STREAM, 0);
		}
		sockaddr_in cldata;
		memset(&cldata, 0, sizeof(sockaddr_in));
		cldata.sin_family = AF_INET;
		while (bind(data_fd, (sockaddr*)&cldata, sizeof(sockaddr)) == -1);
		if ((listen(data_fd, 2)) == -1)
		{
			close(data_fd);
			close(con_fd);
			cout << "Error!\n";
			return -1;
		}
		socklen_t len = sizeof(sockaddr);
		if (getsockname(data_fd, (sockaddr*)&cldata, &len) == -1)
		{
			close(data_fd);
			close(con_fd);
			cout << "Error!\n";
			return -1;
		}
		int port = ntohs(cldata.sin_port);
		char ip[16];
		get_local_ip(ip);
		string sip = ip, d = ".";
		vector<string> sp = split(sip, d);
		string cmd = "PORT " + sp[0] + "," + sp[1] + "," + sp[2] + "," + sp[3] + "," + to_string(port / 256) + "," + to_string(port % 256) + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << cmd;
		}
		while (send(con_fd, cmd.c_str(), cmd.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			cout << "421 Timeout.\n";
			return -1;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			return -1;
		}
		return data_fd;
	}

	void ftp_ls(string remotedirectory = "", string localfile = "", bool app = false)
	{
		if (localfile == "-")
		{
			localfile = "";
		}
		if (bi_mode)
		{
			ftp_ascii();
		}
		if (!connected)
		{
			return;
		}
		if (!remotedirectory.size())
		{
			int data_fd = (pasv_mode) ? data_pasv() : data_actv();
			if (data_fd == -1)
			{
				return;
			}
			if (debug_mode)
			{
				cout << "---> NLST\n";
			}
			while (send(con_fd, "NLST\r\n", 6, 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
			if (!pasv_mode)
			{
				sockaddr_in ser;
				memset(&ser, 0, sizeof(sockaddr_in));
				socklen_t sin = sizeof(sockaddr);
				int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
				if (new_fd == -1)
				{
					cout << "Error\n";
					close(con_fd);
					close(data_fd);
					return;
				}
				close(data_fd);
				data_fd = new_fd;
			}
			while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
			{
				buf[num] = 0;
				cout << buf;
			}
			close(data_fd);
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				return;
			}
		}
		else
		{
			if (!localfile.size())
			{
				int data_fd = (pasv_mode) ? data_pasv() : data_actv();
				if (data_fd == -1)
				{
					return;
				}
				remotedirectory = "NLST " + remotedirectory + "\r\n";
				if (debug_mode)
				{
					cout << "---> " << remotedirectory;
				}
				while (send(con_fd, remotedirectory.c_str(), remotedirectory.size(), 0) == -1);
				int num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					return;
				}
				if (!pasv_mode)
				{
					sockaddr_in ser;
					memset(&ser, 0, sizeof(sockaddr_in));
					socklen_t sin = sizeof(sockaddr);
					int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
					if (new_fd == -1)
					{
						cout << "Error\n";
						close(con_fd);
						close(data_fd);
						return;
					}
					close(data_fd);
					data_fd = new_fd;
				}
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					cout << buf;
				}
				close(data_fd);
				num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					return;
				}
			}
			else
			{
				int data_fd = (pasv_mode) ? data_pasv() : data_actv();
				if (data_fd == -1)
				{
					return;
				}
				remotedirectory = "NLST " + remotedirectory + "\r\n";
				if (debug_mode)
				{
					cout << "---> " << remotedirectory;
				}
				while (send(con_fd, remotedirectory.c_str(), remotedirectory.size(), 0) == -1);
				int num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					return;
				}
				if (!pasv_mode)
				{
					sockaddr_in ser;
					memset(&ser, 0, sizeof(sockaddr_in));
					socklen_t sin = sizeof(sockaddr);
					int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
					if (new_fd == -1)
					{
						cout << "Error\n";
						close(con_fd);
						close(data_fd);
						return;
					}
					close(data_fd);
					data_fd = new_fd;
				}
				ofstream out;
				if (app)
				{
					out.open(localfile, ios::app);
				}
				else
				{
					out.open(localfile);
				}
				if (!out.is_open())
				{
					cout << localfile << ": Can't create the file.\n";
					close(data_fd);
					num = recv(con_fd, buf, BUFSIZE, 0);
					return;
				}
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					rm_r(buf);
					out << buf;
				}
				close(data_fd);
				out.close();
				num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					return;
				}
			}
		}
	}

	void ftp_dir(string remotedirectory = "", string localfile = "", bool app = false)
	{
		if (localfile == "-")
		{
			localfile = "";
		}
		if (bi_mode)
		{
			ftp_ascii();
		}
		if (!connected)
		{
			return;
		}
		if (!remotedirectory.size())
		{
			int data_fd = (pasv_mode) ? data_pasv() : data_actv();
			if (data_fd == -1)
			{
				return;
			}
			if (debug_mode)
			{
				cout << "---> LIST\n";
			}
			while (send(con_fd, "LIST\r\n", 6, 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
			if (!pasv_mode)
			{
				sockaddr_in ser;
				memset(&ser, 0, sizeof(sockaddr_in));
				socklen_t sin = sizeof(sockaddr);
				int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
				if (new_fd == -1)
				{
					cout << "Error\n";
					close(con_fd);
					close(data_fd);
					return;
				}
				close(data_fd);
				data_fd = new_fd;
			}
			while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
			{
				buf[num] = 0;
				cout << buf;
			}
			close(data_fd);
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				return;
			}
		}
		else
		{
			if (!localfile.size())
			{
				int data_fd = (pasv_mode) ? data_pasv() : data_actv();
				if (data_fd == -1)
				{
					return;
				}
				remotedirectory = "LIST " + remotedirectory + "\r\n";
				if (debug_mode)
				{
					cout << "---> " << remotedirectory;
				}
				while (send(con_fd, remotedirectory.c_str(), remotedirectory.size(), 0) == -1);
				int num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					return;
				}
				if (!pasv_mode)
				{
					sockaddr_in ser;
					memset(&ser, 0, sizeof(sockaddr_in));
					socklen_t sin = sizeof(sockaddr);
					int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
					if (new_fd == -1)
					{
						cout << "Error\n";
						close(con_fd);
						close(data_fd);
						return;
					}
					close(data_fd);
					data_fd = new_fd;
				}
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					cout << buf;
				}
				close(data_fd);
				num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					return;
				}
			}
			else
			{
				int data_fd = (pasv_mode) ? data_pasv() : data_actv();
				if (data_fd == -1)
				{
					return;
				}
				remotedirectory = "LIST " + remotedirectory + "\r\n";
				if (debug_mode)
				{
					cout << "---> " << remotedirectory;
				}
				while (send(con_fd, remotedirectory.c_str(), remotedirectory.size(), 0) == -1);
				int num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					close(data_fd);
					return;
				}
				if (!pasv_mode)
				{
					sockaddr_in ser;
					memset(&ser, 0, sizeof(sockaddr_in));
					socklen_t sin = sizeof(sockaddr);
					int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
					if (new_fd == -1)
					{
						cout << "Error\n";
						close(con_fd);
						close(data_fd);
						return;
					}
					close(data_fd);
					data_fd = new_fd;
				}
				ofstream out;
				if (app)
				{
					out.open(localfile, ios::binary | ios::app);
				}
				else
				{
					out.open(localfile, ios::binary);
				}
				if (!out.is_open())
				{
					cout << localfile << ": Can't create the file.\n";
					close(data_fd);
					num = recv(con_fd, buf, BUFSIZE, 0);
					return;
				}
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					rm_r(buf);
					out << buf;
				}
				close(data_fd);
				out.close();
				num = recv(con_fd, buf, BUFSIZE, 0);
				if (num == -1)
				{
					connected = false;
					close(con_fd);
					cout << "421 Timeout.\n";
					return;
				}
				buf[num] = 0;
				cout << buf;
				if (get_code(buf) == 421)
				{
					connected = false;
					close(con_fd);
					return;
				}
			}
		}
	}

	void ftp_recv(string& remotefile, string localfile = "")
	{
		if (!localfile.size())
		{
			localfile = remotefile;
		}
		cout << "local: " << localfile << " remote: " << remotefile << endl;
		int data_fd = (pasv_mode) ? data_pasv() : data_actv();
		if (data_fd == -1)
		{
			return;
		}
		if (offset)
		{
			string rest = "REST " + to_string(offset) + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << rest;
			}
			while (send(con_fd, rest.c_str(), rest.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
		}
		remotefile = "RETR " + remotefile + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remotefile;
		}
		while (send(con_fd, remotefile.c_str(), remotefile.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			cout << "421 Timeout.\n";
			offset = 0;
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			offset = 0;
			return;
		}
		struct timeval t1, t2;
		gettimeofday(&t1, NULL);
		if (get_code(buf) != 150)
		{
			close(data_fd);
			offset = 0;
			return;
		}
		if (!pasv_mode)
		{
			sockaddr_in ser;
			memset(&ser, 0, sizeof(sockaddr_in));
			socklen_t sin = sizeof(sockaddr);
			int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
			if (new_fd == -1)
			{
				cout << "Error\n";
				close(con_fd);
				close(data_fd);
				return;
			}
			close(data_fd);
			data_fd = new_fd;
		}
		string re = buf, d = "(";
		vector<string> sp = split(re, d);
		int len = stoi(sp[1]) - offset;
		bool rv = !stoi(sp[1]) || len > 0;
		if (rv)
		{
			fstream out;
			if (offset)
			{
				out.open(localfile, ios::binary | ios::out | ios::in);
				out.seekp(offset, ios::beg);
			}
			else
			{
				out.open(localfile, ios::binary | ios::out);
			}
			while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
			{
				buf[num] = 0;
				if (!bi_mode)
				{
					num = rm_r(buf);
				}
				out.write(buf, num);
			}
			out.close();
		}
		gettimeofday(&t2, NULL);
		offset = 0;
		close(data_fd);
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		if (rv)
		{
			double deltaT = t2.tv_sec - t1.tv_sec + double(t2.tv_usec - t1.tv_usec) / 1000000;
			double v = len / deltaT;
			vector<string> unit = { " B/s", " kB/s", " MB/s", " GB/s" };
			int index = 0;
			while (v > 1024)
			{
				v /= 1024;
				++index;
			}
			cout << len << " bytes received in " << setprecision(2) << fixed << deltaT << " secs (" << setprecision(4) << fixed << v << unit[index] << ")\n";
		}
	}

	void ftp_send(string& localfile, string remotefile = "")
	{
		if (!remotefile.size())
		{
			remotefile = localfile;
		}
		cout << "local: " << localfile << " remote: " << remotefile << endl;
		ifstream in(localfile);
		if (!in.is_open())
		{
			cout << "local: " << localfile << ": No such file or directory\n";
			return;
		}
		struct stat st;
		stat(localfile.c_str(), &st);
		if(S_ISDIR(st.st_mode))
		{
			cout << "local: " << localfile << ": not a plain file.\n";
			return;
		}
		int len = st.st_size - offset;
		bool rv = !st.st_size || len > 0;
		if (rv)
		{
			in.seekg(offset, ios::beg);
		}
		int data_fd = (pasv_mode) ? data_pasv() : data_actv();
		if (data_fd == -1)
		{
			return;
		}
		if (offset)
		{
			string rest = "REST " + to_string(offset) + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << rest;
			}
			while (send(con_fd, rest.c_str(), rest.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			offset = 0;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
		}
		remotefile = "STOR " + remotefile + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remotefile;
		}
		while (send(con_fd, remotefile.c_str(), remotefile.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			return;
		}
		if (get_code(buf) != 150)
		{
			close(data_fd);
			return;
		}
		if (!pasv_mode)
		{
			sockaddr_in ser;
			memset(&ser, 0, sizeof(sockaddr_in));
			socklen_t sin = sizeof(sockaddr);
			int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
			if (new_fd == -1)
			{
				cout << "Error\n";
				close(con_fd);
				close(data_fd);
				return;
			}
			close(data_fd);
			data_fd = new_fd;
		}
		string tmp;
		int cnt = 0;
		struct timeval t1, t2;
		gettimeofday(&t1, NULL);
		if (rv)
		{
			if (!bi_mode)
			{
				while (getline(in, tmp))
				{
					tmp += "\r\n";
					++cnt;
					while (send(data_fd, tmp.c_str(), tmp.size(), 0) == -1);
				}
			}
			else
			{
				int i;
				for (i = len; i > 0; i -= min(i, BUFSIZE))
				{
					in.read(buf, min(i, BUFSIZE));
					while (send(data_fd, buf, min(i, BUFSIZE), 0) == -1);
				}
			}
		}
		close(data_fd);
		in.close();
		gettimeofday(&t2, NULL);
		len += cnt;
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		if (rv)
		{
			double deltaT = t2.tv_sec - t1.tv_sec + double(t2.tv_usec - t1.tv_usec) / 1000000;
			double v = len / deltaT;
			vector<string> unit = { " B/s", " kB/s", " MB/s", " GB/s" };
			int index = 0;
			while (v > 1024)
			{
				v /= 1024;
				++index;
			}
			cout << len << " bytes sent in " << setprecision(2) << fixed << deltaT << " secs (" << setprecision(4) << fixed << v << unit[index] << ")\n";
		}
	}

	void ftp_mls(vector<string>& remotefiles, string& localfile)
	{
		if (localfile == "-")
		{
			for (auto& rf : remotefiles)
			{
				if (rf == "-")
				{
					ftp_ls();
				}
				else
				{
					ftp_ls(rf);
				}
				if (!connected)
				{
					return;
				}
			}
			return;
		}
		if (interactive_mode)
		{
			cout << "output to localfile: " << localfile << "? ";
			string res;
			cin >> res;
			cin.get();
			if (res == "n" || res == "no" || res == "N" || res == "NO")
			{
				return;
			}
		}
		string emp = "";
		for (int i = 0; i < remotefiles.size(); ++i)
		{
			ftp_ls(remotefiles[i], localfile, i);
			if (!connected)
			{
				return;
			}
		}
	}

	void ftp_mdir(vector<string>& remotefiles, string& localfile)
	{
		if (localfile == "-")
		{
			for (auto& rf : remotefiles)
			{
				if (rf == "-")
				{
					ftp_dir();
				}
				else
				{
					ftp_dir(rf);
				}
				if (!connected)
				{
					return;
				}
			}
			return;
		}
		if (interactive_mode)
		{
			cout << "output to localfile: " << localfile << "? ";
			string res;
			cin >> res;
			cin.get();
			if (res == "n" || res == "no" || res == "N" || res == "NO")
			{
				return;
			}
		}
		for (int i = 0; i < remotefiles.size(); ++i)
		{
			ftp_dir(remotefiles[i], localfile, i);
			if (!connected)
			{
				return;
			}
		}
	}

	void ftp_mdel(vector<string>& remotefiles)
	{
		string rf, tmp, d = "\r\n";
		for (auto r : remotefiles)
		{
			int data_fd = (pasv_mode) ? data_pasv() : data_actv();
			if (data_fd == -1)
			{
				return;
			}
			tmp = "NLST " + r + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << tmp;
			}
			while (send(con_fd, tmp.c_str(), tmp.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
			if (!pasv_mode)
			{
				sockaddr_in ser;
				memset(&ser, 0, sizeof(sockaddr_in));
				socklen_t sin = sizeof(sockaddr);
				int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
				if (new_fd == -1)
				{
					cout << "Error\n";
					close(con_fd);
					close(data_fd);
					return;
				}
				close(data_fd);
				data_fd = new_fd;
			}
			while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
			{
				buf[num] = 0;
				tmp = buf;
				rf += tmp;
			}
			close(data_fd);
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
		}
		vector<string> sp = split(rf, d);
		for (auto f : sp)
		{
			if (interactive_mode)
			{
				cout << "mdelete " << f << "? ";
				string res;
				cin >> res;
				cin.get();
				if (res == "n" || res == "no" || res == "N" || res == "NO")
				{
					continue;
				}
			}
			ftp_delete(f);
		}
	}

	void ftp_size(string& remotefile)
	{
		remotefile = "SIZE " + remotefile + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remotefile;
		}
		while (send(con_fd, remotefile.c_str(), remotefile.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	void ftp_sys()
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		if (debug_mode)
		{
			cout << "---> SYST\n";
		}
		while (send(con_fd, "SYST\r\n", 6, 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
	}

	string eng_to_num(string s)
	{
		if (s == "Jan")
		{
			return "01";
		}
		if (s == "Feb")
		{
			return "02";
		}
		if (s == "Mar")
		{
			return "03";
		}
		if (s == "Apr")
		{
			return "04";
		}
		if (s == "May")
		{
			return "05";
		}
		if (s == "Jun")
		{
			return "06";
		}
		if (s == "Jul")
		{
			return "07";
		}
		if (s == "Aug")
		{
			return "08";
		}
		if (s == "Sep")
		{
			return "09";
		}
		if (s == "Oct")
		{
			return "10";
		}
		if (s == "Nov")
		{
			return "11";
		}
		if (s == "Dec")
		{
			return "12";
		}
		return "";
	}

	string s_time(struct stat& st)
	{
		string ct = ctime(&st.st_mtime), d = " :\r\n";
		vector<string> sp = split(ct, d);
		sp[1] = eng_to_num(sp[1]);
		return sp[6] + sp[1] + sp[2] + sp[3] + sp[4] + sp[5];
	}

	void ftp_newer(string& remotefile, string localfile = "")
	{
		if (!localfile.size())
		{
			localfile = remotefile;
		}
		struct stat st;
		int res = stat(localfile.c_str(), &st);
		if (res)
		{
			ftp_recv(remotefile, localfile);
			return;
		}
		string get_time = "MDTM " + remotefile + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << get_time;
		}
		while (send(con_fd, get_time.c_str(), get_time.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		if (get_code(buf) != 213)
		{
			return;
		}
		string rtime = (buf + 4);
		if (s_time(st) >= rtime)
		{
			cout << "Local file \"" << localfile << "\" is newer than remote file \"" << remotefile << "\"\n";
		}
		else
		{
			ftp_recv(remotefile, localfile);
		}
	}

	void ftp_append(string& localfile, string remotefile = "")
	{
		if (!remotefile.size())
		{
			remotefile = localfile;
		}
		cout << "local: " << localfile << " remote: " << remotefile << endl;
		ifstream in(localfile);
		if (!in.is_open())
		{
			cout << "local: " << localfile << ": No such file or directory\n";
			return;
		}
		struct stat st;
		stat(localfile.c_str(), &st);
		int len = st.st_size - offset;
		bool rv = !st.st_size || len > 0;
		if (rv)
		{
			in.seekg(offset, ios::beg);
		}
		int data_fd = (pasv_mode) ? data_pasv() : data_actv();
		if (data_fd == -1)
		{
			return;
		}
		if (offset)
		{
			string rest = "REST " + to_string(offset) + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << rest;
			}
			while (send(con_fd, rest.c_str(), rest.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			buf[num] = 0;
			cout << buf;
			offset = 0;
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
		}
		remotefile = "APPE " + remotefile + "\r\n";
		if (debug_mode)
		{
			cout << "---> " << remotefile;
		}
		while (send(con_fd, remotefile.c_str(), remotefile.size(), 0) == -1);
		int num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			close(data_fd);
			return;
		}
		if (get_code(buf) != 150)
		{
			close(data_fd);
			return;
		}
		if (!pasv_mode)
		{
			sockaddr_in ser;
			memset(&ser, 0, sizeof(sockaddr_in));
			socklen_t sin = sizeof(sockaddr);
			int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
			if (new_fd == -1)
			{
				cout << "Error\n";
				close(con_fd);
				close(data_fd);
				return;
			}
			close(data_fd);
			data_fd = new_fd;
		}
		string tmp;
		int cnt = 0;
		struct timeval t1, t2;
		gettimeofday(&t1, NULL);
		if (rv)
		{
			if (!bi_mode)
			{
				while (getline(in, tmp))
				{
					tmp += "\r\n";
					++cnt;
					while (send(data_fd, tmp.c_str(), tmp.size(), 0) == -1);
				}
			}
			else
			{
				int i;
				for (i = len; i > 0; i -= min(i, BUFSIZE))
				{
					in.read(buf, min(i, BUFSIZE));
					while (send(data_fd, buf, min(i, BUFSIZE), 0) == -1);
				}
			}
		}
		close(data_fd);
		in.close();
		gettimeofday(&t2, NULL);
		len += cnt;
		num = recv(con_fd, buf, BUFSIZE, 0);
		if (num == -1)
		{
			connected = false;
			close(con_fd);
			cout << "421 Timeout.\n";
			return;
		}
		buf[num] = 0;
		cout << buf;
		if (get_code(buf) == 421)
		{
			connected = false;
			close(con_fd);
			return;
		}
		if (rv)
		{
			double deltaT = t2.tv_sec - t1.tv_sec + double(t2.tv_usec - t1.tv_usec) / 1000000;
			double v = len / deltaT;
			vector<string> unit = { " B/s", " kB/s", " MB/s", " GB/s" };
			int index = 0;
			while (v > 1024)
			{
				v /= 1024;
				++index;
			}
			cout << len << " bytes sent in " << setprecision(2) << fixed << deltaT << " secs (" << setprecision(4) << fixed << v << unit[index] << ")\n";
		}
	}

	void ftp_restart(string& off)
	{
		if (!connected)
		{
			cout << "Not connected.\n";
			return;
		}
		offset = stoi(off);
		cout << "restarting at " << offset << ". execute get, put or append to initiate transfer\n";
	}

	void ftp_reget(string& remotefile, string localfile = "")
	{
		if (!localfile.size())
		{
			localfile = remotefile;
		}
		struct stat st;
		int res = stat(localfile.c_str(), &st);
		if (res)
		{
			cout << "local: " << localfile << ": No such file or directory\n";
			return;
		}
		offset = st.st_size;
		ftp_recv(remotefile, localfile);
	}

	void ftp_mput(vector<string>& localfiles)
	{
		for (auto& s : localfiles)
		{
			if (interactive_mode)
			{
				cout << "mput " << s << "? ";
				string res;
				cin >> res;
				cin.get();
				if (res == "n" || res == "no" || res == "N" || res == "NO")
				{
					continue;
				}
			}
			ftp_send(s);
			if (!connected)
			{
				return;
			}
		}
	}

	void ftp_mget(vector<string>& remotefiles)
	{
		string rf, tmp, d = "\r\n";
		for (auto& r : remotefiles)
		{
			int data_fd = (pasv_mode) ? data_pasv() : data_actv();
			if (data_fd == -1)
			{
				return;
			}
			tmp = "NLST " + r + "\r\n";
			if (debug_mode)
			{
				cout << "---> " << tmp;
			}
			while (send(con_fd, tmp.c_str(), tmp.size(), 0) == -1);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
			if (!pasv_mode)
			{
				sockaddr_in ser;
				memset(&ser, 0, sizeof(sockaddr_in));
				socklen_t sin = sizeof(sockaddr);
				int new_fd = accept(data_fd, (sockaddr*)&ser, &sin);
				if (new_fd == -1)
				{
					cout << "Error\n";
					close(con_fd);
					close(data_fd);
					return;
				}
				close(data_fd);
				data_fd = new_fd;
			}
			while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
			{
				buf[num] = 0;
				tmp = buf;
				rf += tmp;
			}
			close(data_fd);
			num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				cout << "421 Timeout.\n";
				return;
			}
			if (get_code(buf) == 421)
			{
				connected = false;
				close(con_fd);
				close(data_fd);
				return;
			}
		}
		vector<string> sp = split(rf, d);
		for (auto& f : sp)
		{
			if (interactive_mode)
			{
				cout << "mget " << f << "? ";
				string res;
				cin >> res;
				cin.get();
				if (res == "n" || res == "no" || res == "N" || res == "NO")
				{
					continue;
				}
			}
			ftp_recv(f);
			if (!connected)
			{
				return;
			}
		}
	}

public:
	client()
	{
		srand(time(0));
		memset(&ser_addr, 0, sizeof(sockaddr_in));
		debug_mode = connected = false;
		pasv_mode = bi_mode = interactive_mode = true;
		offset = 0;
		eth = "wlo1";
	}

	client(const char* __eth)
	{
		srand(time(0));
		debug_mode = connected = false;
		pasv_mode = bi_mode = interactive_mode = true;
		offset = 0;
		eth = __eth;
	}

	client(const char* __eth, const char* ip)
	{
		srand(time(0));
		debug_mode = connected = false;
		pasv_mode = bi_mode = interactive_mode = true;
		offset = 0;
		eth = __eth;
		ser_ip = ip;
		ftp_connect(ip);
		login();
	}

	void shell()
	{
		string command, d = " ";
		vector<string> cmd;
		vector<string> cmd_list = { "!", "debug", "ascii", "binary", "type", "close", "quit", "bye", "help", "disconnect", "status", "lcd", "passive", "open", "user", "cd", "pwd", "mkdir", "rmdir", "remotehelp", "rename", "quote", "literal", "prompt", "delete", "ls", "dir", "recv", "get", "send", "put", "mls", "mdir", "mdelete", "size", "system", "newer", "append", "restart", "reget", "mput", "mget" };
		vector<string> cmd_list_hidden = { "bi", "asc" };
		sort(cmd_list.begin(), cmd_list.end());
		while (true)
		{
			cout << "ftp> ";
			getline(cin, command);
			if (command.size() && command[0] == '!')
			{
				if (debug_mode)
				{
					cout << "/bin/bash\n";
				}
				int r = system(command.substr(1, command.size() - 1).c_str());
				continue;
			}
			cmd = split(command, d);
			if (cmd.size() == 0)
			{
				continue;
			}
			if (vector_find(cmd_list, cmd[0]) == cmd_list.size() && vector_find(cmd_list_hidden, cmd[0]) == cmd_list_hidden.size())
			{
				cout << "?Invalid command\n";
				continue;
			}
			if (cmd[0] == "debug")
			{
				if (cmd.size() == 1)
				{
					ftp_debug(!debug_mode);
				}
				else
				{
					if (is_num(cmd[1]) && stoi(cmd[1]))
					{
						ftp_debug(1);
					}
					else
					{
						ftp_debug(0);
					}
				}
			}
			if (cmd[0] == "ascii" || cmd[0] == "asc")
			{
				ftp_ascii();
			}
			if (cmd[0] == "binary" || cmd[0] == "bi")
			{
				ftp_bi();
			}
			if (cmd[0] == "type")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "Using ";
					if (bi_mode)
					{
						cout << "binary";
					}
					else
					{
						cout << "ascii";
					}
					cout << " mode to transfer files.\n";
				}
				else
				{
					if (cmd.size() == 2)
					{
						if (cmd[1] != "ascii" && cmd[1] != "binary")
						{
							cout << cmd[1] << ": Unknown mode\n";
						}
						if (cmd[1] == "ascii")
						{
							ftp_ascii();
						}
						if (cmd[1] == "binary")
						{
							ftp_bi();
						}
					}
					else
					{
						cout << "Usage: type [ ascii | binary ]\n";
					}
				}
			}
			if (cmd[0] == "close" || cmd[0] == "disconnect")
			{
				ftp_close();
			}
			if (cmd[0] == "quit" || cmd[0] == "bye")
			{
				if (!connected)
				{
					break;
				}
				if (debug_mode)
				{
					cout << "---> QUIT\n";
				}
				while (send(con_fd, "QUIT\r\n", 6, 0) == -1);
				int num = recv(con_fd, buf, BUFSIZE, 0);
				buf[num] = 0;
				cout << buf;
				connected = false;
				close(con_fd);
				break;
			}
			if (cmd[0] == "help")
			{
				cout << "Commands may be abbreviated. \nCommands are: \n";
				int i;
				for (i = 0; i < cmd_list.size(); ++i)
				{
					cout << setw(16) << left << cmd_list[i];
					if (i % 5 == 4)
					{
						cout << endl;
					}
				}
				cout << "\nTo get the usage of each command, please visit https://docs.microsoft.com/en-us/windows-server/administration/windows-commands/ftp" << endl;
			}
			if (cmd[0] == "status")
			{
				ftp_status();
			}
			if (cmd[0] == "lcd")
			{
				if (cmd.size() > 2)
				{
					cout << "usage: lcd local-directory\n";
				}
				if (cmd.size() == 1)
				{
					char path[255];
					cout << "Local directory now " << getcwd(path, 255) << endl;
				}
				if (cmd.size() == 2)
				{
					ftp_lcd(cmd[1]);
				}
			}
			if (cmd[0] == "passive")
			{
				ftp_pasv();
			}
			if (cmd[0] == "open")
			{
				if (connected)
				{
					cout << "Already connected to " << inet_ntoa(ser_addr.sin_addr) << ", use close first.\n";
					continue;
				}
				if (cmd.size() > 2)
				{
					cout << "usage: open host-name\n";
				}
				if (cmd.size() == 1)
				{
					cout << "(to) ";
					string ip;
					getline(cin, ip);
					vector<string> sp = split(ip, d);
					if (sp.size() != 1)
					{
						cout << "usage: open host-name\n";
					}
					else
					{
						ftp_open(ip.c_str());
					}
				}
				if (cmd.size() == 2)
				{
					ftp_open(cmd[1].c_str());
				}
			}
			if (cmd[0] == "user")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() > 4)
				{
					cout << "usage: user username [password] [account]\n";
				}
				if (cmd.size() == 1)
				{
					cout << "(username) ";
					string user;
					getline(cin, user);
					vector<string> sp = split(user, d);
					if (sp.size() > 3)
					{
						cout << "usage: user username [password] [account]\n";
					}
					else
					{
						ftp_user(user);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_user(cmd[1]);
				}
				if (cmd.size() == 3)
				{
					ftp_user(cmd[1], cmd[2]);
				}
				if (cmd.size() == 4)
				{
					ftp_user(cmd[1], cmd[2], cmd[3]);
				}
			}
			if (cmd[0] == "cd")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-directory) ";
					string remote_path;
					getline(cin, remote_path);
					vector<string> sp = split(remote_path, d);
					if (!sp.size())
					{
						cout << "usage: cd remote-directory\n";
						continue;
					}
					ftp_cd(sp[0]);
				}
				else
				{
					ftp_cd(cmd[1]);
				}
			}
			if (cmd[0] == "pwd")
			{
				ftp_pwd();
			}
			if (cmd[0] == "mkdir")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(directory-name) ";
					string dir;
					getline(cin, dir);
					vector<string> sp = split(dir, d);
					if (!sp.size())
					{
						cout << "usage: mkdir directory-name\n";
						continue;
					}
					ftp_mkdir(sp[0]);
				}
				else
				{
					ftp_mkdir(cmd[1]);
				}
			}
			if (cmd[0] == "rmdir")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(directory-name) ";
					string dir;
					getline(cin, dir);
					vector<string> sp = split(dir, d);
					if (!sp.size())
					{
						cout << "usage: rmdir directory-name\n";
						continue;
					}
					ftp_rmdir(sp[0]);
				}
				else
				{
					ftp_rmdir(cmd[1]);
				}
			}
			if (cmd[0] == "remotehelp")
			{
				if (cmd.size() == 1)
				{
					ftp_remotehelp();
				}
				else
				{
					ftp_remotehelp(cmd[1]);
				}
			}
			if (cmd[0] == "rename")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(from-name) ";
					string from;
					getline(cin, from);
					vector<string> sp = split(from, d);
					if (!sp.size())
					{
						cout << "rename from-name to-name\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(to-name) ";
						string to;
						getline(cin, to);
						vector<string> spto = split(to, d);
						if (!spto.size())
						{
							cout << "rename from-name to-name\n";
							continue;
						}
						ftp_rename(sp[0], spto[0]);
					}
					else
					{
						ftp_rename(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					cout << "(to-name) ";
					string to;
					getline(cin, to);
					vector<string> spto = split(to, d);
					if (!spto.size())
					{
						cout << "rename from-name to-name\n";
						continue;
					}
					ftp_rename(cmd[1], spto[0]);
				}
				if (cmd.size() > 2)
				{
					ftp_rename(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "quote" || cmd[0] == "literal")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(command line to send) ";
					string cl;
					getline(cin, cl);
					vector<string> sp = split(cl, d);
					if (!sp.size())
					{
						cout << "usage: " << cmd[0] << " line-to-send\n";
						continue;
					}
					cl = "";
					for (auto& ele : sp)
					{
						cl += ele + " ";
					}
					cl.pop_back();
					ftp_quote(cl);
				}
				else
				{
					command = "";
					int i;
					for (i = 1; i < cmd.size(); ++i)
					{
						command += cmd[i];
						if (i != cmd.size() - 1)
						{
							command.push_back(' ');
						}
					}
					ftp_quote(command);
				}
			}
			if (cmd[0] == "prompt")
			{
				ftp_prompt();
			}
			if (cmd[0] == "delete")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-file) ";
					string remote_file;
					getline(cin, remote_file);
					vector<string> sp = split(remote_file, d);
					if (!sp.size())
					{
						cout << "usage: delete remote-file\n";
						continue;
					}
					ftp_delete(sp[0]);
				}
				else
				{
					ftp_delete(cmd[1]);
				}
			}
			if (cmd[0] == "ls")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					ftp_ls();
				}
				if (cmd.size() == 2)
				{
					ftp_ls(cmd[1]);
				}
				if (cmd.size() == 3)
				{
					if (interactive_mode && cmd[2] != "-")
					{
						cout << "output to localfile: " << cmd[2] << "? ";
						string res;
						cin >> res;
						cin.get();
						if (res == "n" || res == "no" || res == "N" || res == "NO")
						{
							continue;
						}
					}
					ftp_ls(cmd[1], cmd[2]);
				}
				if (cmd.size() > 3)
				{
					cout << "usage: ls remote-directory local-file\n";
				}
			}
			if (cmd[0] == "dir")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					ftp_dir();
				}
				if (cmd.size() == 2)
				{
					ftp_dir(cmd[1]);
				}
				if (cmd.size() == 3)
				{
					if (interactive_mode && cmd[2] != "-")
					{
						cout << "output to localfile: " << cmd[2] << "? ";
						string res;
						cin >> res;
						cin.get();
						if (res == "n" || res == "no" || res == "N" || res == "NO")
						{
							continue;
						}
					}
					ftp_dir(cmd[1], cmd[2]);
				}
				if (cmd.size() > 3)
				{
					cout << "usage: dir remote-directory local-file\n";
				}
			}
			if (cmd[0] == "recv" || cmd[0] == "get")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-file) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: " << cmd[0] << " remote-file [ local-file ]\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(local-file) ";
						string localfile;
						getline(cin, localfile);
						vector<string> splo = split(localfile, d);
						if (!splo.size())
						{
							cout << "usage: " << cmd[0] << " remote-file [ local-file ]\n";
							continue;
						}
						ftp_recv(sp[0], splo[0]);
					}
					else
					{
						ftp_recv(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_recv(cmd[1]);
				}
				if (cmd.size() > 2)
				{
					ftp_recv(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "send" || cmd[0] == "put")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(local-file) ";
					string localfile;
					getline(cin, localfile);
					vector<string> sp = split(localfile, d);
					if (!sp.size())
					{
						cout << "usage: " << cmd[0] << " local-file [ remote-file ]\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(remote-file) ";
						string remotefile;
						getline(cin, remotefile);
						vector<string> spre = split(remotefile, d);
						if (!spre.size())
						{
							cout << "usage: " << cmd[0] << " local-file [ remote-file ]\n";
							continue;
						}
						ftp_send(sp[0], spre[0]);
					}
					else
					{
						ftp_send(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_send(cmd[1]);
				}
				if (cmd.size() > 2)
				{
					ftp_send(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "mls")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-files) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: mls remote-files local-file\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(local-file) ";
						string localfile;
						getline(cin, localfile);
						vector<string> splo = split(localfile, d);
						if (!splo.size())
						{
							cout << "usage: mls remote-files local-file\n";
							continue;
						}
						vector<string> rfs(sp.begin(), sp.end());
						int i;
						for (i = 0; i < splo.size() - 1; ++i)
						{
							rfs.push_back(splo[i]);
						}
						ftp_mls(rfs, splo[i]);
					}
					else
					{
						vector<string> rfs(sp.begin(), sp.end() - 1);
						ftp_mls(rfs, sp[sp.size() - 1]);
					}
				}
				if (cmd.size() >= 2)
				{
					vector<string> rfs(cmd.begin() + 1, cmd.end() - 1);
					ftp_mls(rfs, cmd[cmd.size() - 1]);
				}
			}
			if (cmd[0] == "mdir")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-files) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: mdir remote-files local-file\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(local-file) ";
						string localfile;
						getline(cin, localfile);
						vector<string> splo = split(localfile, d);
						if (!splo.size())
						{
							cout << "usage: mls remote-files local-file\n";
							continue;
						}
						vector<string> rfs(sp.begin(), sp.end());
						int i;
						for (i = 0; i < splo.size() - 1; ++i)
						{
							rfs.push_back(splo[i]);
						}
						ftp_mdir(rfs, splo[i]);
					}
					else
					{
						vector<string> rfs(sp.begin(), sp.end() - 1);
						ftp_mdir(rfs, sp[sp.size() - 1]);
					}
				}
				if (cmd.size() >= 2)
				{
					vector<string> rfs(cmd.begin() + 1, cmd.end() - 1);
					ftp_mdir(rfs, cmd[cmd.size() - 1]);
				}
			}
			if (cmd[0] == "mdelete")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-files) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: mdelete remote-files\n";
						continue;
					}
					ftp_mdel(sp);
				}
				else
				{
					vector<string> rfs(cmd.begin() + 1, cmd.end());
					ftp_mdel(rfs);
				}
			}
			if (cmd[0] == "size")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(filename) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: size filename\n";
						continue;
					}
					ftp_size(sp[0]);
				}
				else
				{
					ftp_size(cmd[1]);
				}
			}
			if (cmd[0] == "system")
			{
				ftp_sys();
			}
			if (cmd[0] == "newer")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-file) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: newer remote-file [ local-file ]\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(local-file) ";
						string localfile;
						getline(cin, localfile);
						vector<string> splo = split(localfile, d);
						if (!splo.size())
						{
							cout << "usage: newer remote-file [ local-file ]\n";
							continue;
						}
						ftp_newer(sp[0], splo[0]);
					}
					else
					{
						ftp_newer(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_newer(cmd[1]);
				}
				if (cmd.size() > 2)
				{
					ftp_newer(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "append")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(local-file) ";
					string localfile;
					getline(cin, localfile);
					vector<string> sp = split(localfile, d);
					if (!sp.size())
					{
						cout << "usage: append local-file remote-file\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(remote-file) ";
						string remotefile;
						getline(cin, remotefile);
						vector<string> spre = split(remotefile, d);
						if (!spre.size())
						{
							cout << "usage: append local-file remote-file\n";
							continue;
						}
						ftp_append(sp[0], spre[0]);
					}
					else
					{
						ftp_append(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_append(cmd[1]);
				}
				if (cmd.size() > 2)
				{
					ftp_append(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "restart")
			{
				if (cmd.size() != 2)
				{
					cout << "restart: offset not specified\n";
					continue;
				}
				ftp_restart(cmd[1]);
			}
			if (cmd[0] == "reget")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-file) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: " << cmd[0] << " remote-file [ local-file ]\n";
						continue;
					}
					if (sp.size() == 1)
					{
						cout << "(local-file) ";
						string localfile;
						getline(cin, localfile);
						vector<string> splo = split(localfile, d);
						if (!splo.size())
						{
							cout << "usage: " << cmd[0] << " remote-file [ local-file ]\n";
							continue;
						}
						ftp_reget(sp[0], splo[0]);
					}
					else
					{
						ftp_reget(sp[0], sp[1]);
					}
				}
				if (cmd.size() == 2)
				{
					ftp_reget(cmd[1]);
				}
				if (cmd.size() > 2)
				{
					ftp_reget(cmd[1], cmd[2]);
				}
			}
			if (cmd[0] == "mput")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(local-files) ";
					string localfile;
					getline(cin, localfile);
					vector<string> sp = split(localfile, d);
					if (!sp.size())
					{
						cout << "usage: mput local-files\n";
						continue;
					}
					ftp_mput(sp);
				}
				else
				{
					vector<string> lfs(cmd.begin() + 1, cmd.end());
					ftp_mput(lfs);
				}
			}
			if (cmd[0] == "mget")
			{
				if (!connected)
				{
					cout << "Not connected.\n";
					continue;
				}
				if (cmd.size() == 1)
				{
					cout << "(remote-files) ";
					string remotefile;
					getline(cin, remotefile);
					vector<string> sp = split(remotefile, d);
					if (!sp.size())
					{
						cout << "usage: mget remote-files\n";
						continue;
					}
					ftp_mget(sp);
				}
				else
				{
					vector<string> rfs(cmd.begin() + 1, cmd.end());
					ftp_mget(rfs);
				}
			}
		}
	}
};

int main(int argc, char* argv[])
{
	if (argc > 3)
	{
		cout << "Usage: ./FILENAME [NIC name] [hostname]\n";
		return 1;
	}
	if (argc == 1)
	{
		cout << "If you don't specify the NIC name, the active mode may be unavailable.\n";
		cout << "Usage: ./FILENAME [NIC name] [hostname]\n";
		client cl;
		cl.shell();
	}
	else
	{
		if (argc == 2)
		{
			client cl(argv[1]);
			cl.shell();
		}
		else
		{
			client cl(argv[1], argv[2]);
			cl.shell();
		}
	}
	return 0;
}