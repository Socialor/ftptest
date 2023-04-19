//不含NAT穿透功能的服务器程序
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
#include <grp.h>

using namespace std;

#define BUFSIZE 1024
#define BACKLOG 100

int user_cnt = 0, lis_fd, con_fd_2;
string eth, un, pw;

void sigfun(int signum)
{
	if (signum == 16)
	{
		--user_cnt;
	}
	if (signum == 2 || signum == 3)
	{
		close(lis_fd);
		exit(0);
	}
	if (signum == 14)
	{
		string res = "421 Time out.\r\n";
		while (send(con_fd_2, res.c_str(), res.size(), 0) == -1);
		kill(getppid(), 16);
		exit(0);
	}
}


class server 
{
	int con_fd, data_fd, offset;
	sockaddr_in cl_addr, data_addr;
	bool data_pasv, data_actv, login, bi_mode;
	char buf[BUFSIZE];
	string username, fr_name;

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

	bool is_num(string& s)
	{
		for (auto& ch : s)
		{
			if (ch > '9' || ch < '0')
			{
				return false;
			}
		}
		return true;
	}

	string get_mode(mode_t& mode)
	{
		string ans = "";
		char type[7] = { 'p' , 'c' , 'd' , 'b' , '-' , 'l' , 's' };
		int index = ((mode >> 12) & 15) / 2;
		ans.push_back(type[index]);
		vector<string> perm = { "---" , "--x" , "-w-" , "-wx" , "r--" , "r-x" , "rw-" , "rwx" };
		ans += perm[mode >> 6 & 7];
		ans += perm[mode >> 3 & 7];
		ans += perm[mode & 7];
		return ans;
	}

	int get_subdir(string path)
	{
		int cnt = 0;
		DIR* pDir;
		struct dirent* pEnt;
		struct stat st;
		pDir = opendir(path.c_str());
		while (pEnt = readdir(pDir))
		{
			stat(pEnt->d_name, &st);
			if (S_ISDIR(st.st_mode))
			{
				++cnt;
			}
		}
		return cnt;
	}

	string get_detailed_info(string name)
	{
		string res, tmp;
		struct stat st;
		int i;
		stat(name.c_str(), &st);
		res += get_mode(st.st_mode);
		if (S_ISDIR(st.st_mode))
		{
			tmp = to_string(get_subdir(name));
		}
		else
		{
			tmp = to_string(st.st_nlink);
		}
		for (i = 0; i < 5 - tmp.size(); ++i)
		{
			res.push_back(' ');
		}
		res += tmp;
		res.push_back(' ');
		struct passwd* user;
		user = getpwuid(st.st_uid);
		tmp = user->pw_name;
		res += tmp;
		for (i = 0; i < 9 - tmp.size(); ++i)
		{
			res.push_back(' ');
		}
		struct group* group;
		group = getgrgid(st.st_gid);
		tmp = group->gr_name;
		res += tmp;
		for (i = 0; i < 9 - tmp.size(); ++i)
		{
			res.push_back(' ');
		}
		tmp = to_string(st.st_size);
		for (i = 0; i < 9 - tmp.size(); ++i)
		{
			res.push_back(' ');
		}
		res += tmp;
		res.push_back(' ');
		tmp = ctime(&st.st_mtime);
		res += tmp.substr(4, 12);
		res.push_back(' ');
		string d = "/";
		vector<string> sp = split(name, d);
		tmp = sp.back();
		res += tmp;
		return res;
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

	void bye()
	{
		close(con_fd);
		kill(getppid(), 16);
		exit(0);
	}

	void acpt()
	{
		sockaddr_in cl;
		memset(&cl, 0, sizeof(sockaddr_in));
		socklen_t sin = sizeof(sockaddr);
		int new_fd = accept(data_fd, (sockaddr*)&cl, &sin);
		if (new_fd == -1)
		{
			cout << "Error!\n";
			close(con_fd);
			bye();
		}
		close(data_fd);
		data_fd = new_fd;
		data_pasv = false;
	}

	void cnct()
	{
		data_fd = socket(AF_INET, SOCK_STREAM, 0);
		while (data_fd == -1)
		{
			data_fd = socket(AF_INET, SOCK_STREAM, 0);
		}
		int opt = 1;
		setsockopt(data_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
		sockaddr_in cldata;
		memset(&cldata, 0, sizeof(sockaddr_in));
		cldata.sin_family = AF_INET;
		cldata.sin_port = htons(20);
		if (bind(data_fd, (sockaddr*)&cldata, sizeof(sockaddr)) == -1)
		{
			cout << "Cannot bind to port 20.\n";
			close(data_fd);
			bye();
		}
		if (connect(data_fd, (sockaddr*)&data_addr, sizeof(sockaddr)) == -1)
		{
			close(data_fd);
			bye();
		}
		data_actv = false;
	}

public:
	server(int __con_fd, sockaddr_in& __cl_addr)
	{
		con_fd = con_fd_2 = __con_fd;
		cl_addr = __cl_addr;
		data_fd = -1;
		data_pasv = data_actv = login = bi_mode = false;
		username = fr_name = "";
		offset = 0;
		timeval timeo = { 300, 0 };
		setsockopt(con_fd, SOL_SOCKET, SO_SNDTIMEO, &timeo, sizeof(timeo));
		setsockopt(con_fd, SOL_SOCKET, SO_RCVTIMEO, &timeo, sizeof(timeo));
		strcpy(buf, "220 (ylxFTP 1.0.0)\r\n");
		while (send(con_fd, buf, strlen(buf), 0) == -1);
	}

	void ser_main() 
	{
		while (true)
		{
			alarm(0);
			alarm(300);
			int num = recv(con_fd, buf, BUFSIZE, 0);
			if (num == -1)
			{
				strcpy(buf, "421 Timeout.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
				bye();
			}
			buf[num] = 0;
			string msg = buf, cmd, para;
			msg = msg.substr(0, msg.size() - 2);
			int pos = 0;
			while (pos < msg.size() && msg[pos] != ' ')
			{
				++pos;
			}
			cmd = msg.substr(0, pos);
			transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
			para = (pos == msg.size()) ? "" : msg.substr(pos + 1, msg.size() - pos - 1);
			vector<string> cmd_list = { "ABOR","ACCT","ALLO","APPE","CDUP","CWD","DELE","FEAT","HELP","LIST","MDTM","MKD","NLST","NOOP","OPTS","PASS","PASV","PORT","PWD","QUIT","REIN","REST","RETR","RMD","RNFR","RNTO","SIZE","STAT","STOR","STOU","SYST","TYPE","USER","XCUP","XCWD","XMKD","XPWD","XRMD"};
			if (vector_find(cmd_list, cmd) == cmd_list.size())
			{
				strcpy(buf, "500 Unknown Command.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
				continue;
			}
			if (!login && cmd != "USER" && cmd != "PASS" && cmd != "OPTS" && cmd != "QUIT")
			{
				strcpy(buf, "530 Please login with USER and PASS.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
				continue;
			}
			if (cmd == "ABOR")
			{
				if (!data_pasv && !data_actv)
				{
					strcpy(buf, "225 No transfer to ABOR.\r\n");
					while (send(con_fd, buf, strlen(buf), 0) == -1);
				}
				else
				{
					data_pasv = data_actv = false;
					close(data_fd);
					strcpy(buf, "226 ABOR command successful.\r\n");
					while (send(con_fd, buf, strlen(buf), 0) == -1);
				}
			}
			if (cmd == "ACCT" || cmd == "OPTS")
			{
				string res = "502 " + cmd + " not implemented.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
			}
			if (cmd == "ALLO")
			{
				strcpy(buf, "202 ALLO command ignored.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "APPE")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					offset = 0;
					continue;
				}
				ofstream out;
				struct stat st;
				if (stat(para.c_str(), &st) != -1)
				{
					out.open(para, ios::binary | ios::app);
					out.seekp(0, ios::end);
				}
				else
				{
					out.open(para, ios::binary);
				}
				if (data_actv)
				{
					cnct();
				}
				else
				{
					acpt();
				}
				res = "150 Ok to send data.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				int num;
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					out.write(buf, num);
				}
				res = "226 Transfer complete.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				close(data_fd);
				out.close();
				offset = 0;
			}
			if (cmd == "CDUP" || cmd == "XCUP")
			{
				chdir("..");
				strcpy(buf, "250 Directory successfully changed.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "CWD" || cmd == "XCWD")
			{
				if (chdir(para.c_str()) == -1)
				{
					strcpy(buf, "550 Failed to change directory.\r\n");
				}
				else
				{
					strcpy(buf, "250 Directory successfully changed.\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "DELE")
			{
				struct stat st;
				int res = stat(para.c_str(), &st);
				if (res == -1 || S_ISDIR(st.st_mode) || remove(para.c_str()) == -1)
				{
					strcpy(buf, "550 Delete operation failed.\r\n");
				}
				else
				{
					strcpy(buf, "250 Delete operation successful.\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "FEAT")
			{
				strcpy(buf, "211-Features:\r\n MDTM\r\n PASV\r\n PORT\r\n REST STREAM\r\n SIZE\r\n221 End\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "HELP")
			{
				string res = "214-The following commands are recognized.\r\n";
				int i;
				for (i = 0; i < cmd_list.size(); ++i)
				{
					res += " " + cmd_list[i];
					if (cmd_list[i].size() == 3)
					{
						res.push_back(' ');
					}
					if (i % 14 == 13)
					{
						res += "\r\n";
					}
				}
				res += "\r\n214 Help OK.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
			}
			if (cmd == "LIST")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					continue;
				}
				if (data_actv)
				{
					cnct();
				}
				else
				{
					acpt();
				}
				res = "150 Here comes the directory listing.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				res = "";
				if (!para.size() || para[0] == '-')
				{
					bool a = false;
					for (auto& ch : para)
					{
						if (ch == 'a')
						{
							a = true;
						}
					}
					DIR* pDir;
					struct dirent* pEnt;
					char path[255];
					getcwd(path, 255);
					pDir = opendir(path);
					while (pEnt = readdir(pDir))
					{
						if (!a && pEnt->d_name[0] == '.')
						{
							continue;
						}
						res += get_detailed_info(pEnt->d_name) + "\r\n";
						if (res.size() > BUFSIZE)
						{
							while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							res = "";
						}
					}
					if (res.size())
					{
						while (send(data_fd, res.c_str(), res.size(), 0) == -1);
					}
				}
				else
				{
					struct stat st;
					if (stat(para.c_str(), &st) != -1)
					{
						if (S_ISDIR(st.st_mode))
						{
							DIR* pDir;
							struct dirent* pEnt;
							pDir = opendir(para.c_str());
							string newname;
							while (pEnt = readdir(pDir))
							{
								if (pEnt->d_name[0] == '.')
								{
									continue;
								}
								newname = pEnt->d_name;
								newname = para + ((para.back() == '/') ? "" : "/") + newname;
								res += get_detailed_info(newname) + "\r\n";
								if (res.size() > BUFSIZE)
								{
									while (send(data_fd, res.c_str(), res.size(), 0) == -1);
									res = "";
								}
							}
							if (res.size())
							{
								while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							}
						}
						else
						{
							res = get_detailed_info(para.c_str()) + "\r\n";
							if (res.size() > 2)
							{
								while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							}
						}
					}
				}
				close(data_fd);
				res = "226 Directory send OK.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
			}
			if (cmd == "MDTM")
			{
				struct stat st;
				int res = stat(para.c_str(), &st);
				if (res == -1 || S_ISDIR(st.st_mode))
				{
					strcpy(buf, "550 Could not get file modification time.\r\n");
				}
				else
				{
					strcpy(buf, "213 ");
					strcat(buf, s_time(st).c_str());
					strcat(buf, "\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "MKD" || cmd == "XMKD")
			{
				if (mkdir(para.c_str(), 0755) == -1)
				{
					strcpy(buf, "550 Create directory operation failed.\r\n");
				}
				else
				{
					strcpy(buf, "257 \"");
					if (para[0] == '/')
					{
						strcat(buf, para.c_str());
					}
					else
					{
						char path[255];
						getcwd(path, 255);
						strcat(buf, path);
						if (strlen(path) != 1)
						{
							strcat(buf, "/");
						}
						strcat(buf, para.c_str());
					}
					strcat(buf, "\" create\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "NLST")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					continue;
				}
				if (data_actv)
				{
					cnct();
				}
				else
				{
					acpt();
				}
				res = "150 Here comes the directory listing.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				res = "";
				if (!para.size() || para[0] == '-')
				{
					bool a = false, l = false;
					for (auto& ch : para)
					{
						if (ch == 'a')
						{
							a = true;
						}
						if (ch == 'l')
						{
							l = true;
						}
					}
					DIR* pDir;
					struct dirent* pEnt;
					char path[255];
					getcwd(path, 255);
					pDir = opendir(path);
					string dn;
					while (pEnt = readdir(pDir))
					{
						if (!a && pEnt->d_name[0] == '.')
						{
							continue;
						}
						dn = pEnt->d_name;
						if (l)
						{
							res += get_detailed_info(dn) + "\r\n";
						}
						else
						{
							res += dn + "\r\n";
						}
						if (res.size() > BUFSIZE)
						{
							while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							res = "";
						}
					}
					if (res.size())
					{
						while (send(data_fd, res.c_str(), res.size(), 0) == -1);
					}
				}
				else
				{
					struct stat st;
					if (stat(para.c_str(), &st) != -1)
					{
						if (S_ISDIR(st.st_mode))
						{
							DIR* pDir;
							struct dirent* pEnt;
							pDir = opendir(para.c_str());
							string newname;
							while (pEnt = readdir(pDir))
							{
								if (pEnt->d_name[0] == '.')
								{
									continue;
								}
								newname = pEnt->d_name;
								newname = para + ((para.back() == '/') ? "" : "/") + newname;
								res += newname + "\r\n";
								if (res.size() > BUFSIZE)
								{
									while (send(data_fd, res.c_str(), res.size(), 0) == -1);
									res = "";
								}
							}
							if (res.size())
							{
								while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							}
						}
						else
						{
							res = para + "\r\n";
							if (res.size() > 2)
							{
								while (send(data_fd, res.c_str(), res.size(), 0) == -1);
							}
						}
					}
				}
				close(data_fd);
				res = "226 Directory send OK.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
			}
			if (cmd == "NOOP")
			{
				strcpy(buf, "200 NOOP ok.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "PASS")
			{
				string res;
				if (login)
				{
					res = "230 Already logged in.\r\n";
				}
				else
				{
					if (username == "")
					{
						res = "503 Login with USER first.\r\n";
					}
					else
					{
						passwd* pwd;
						pwd = getpwuid(getuid());
						if (username == un && para == pw)
						{
							res = "230 Login successful.\r\n";
							login = true;
						}
						else
						{
							res = "530 Login incorrect.\r\n";
							username = "";
						}
					}
				}
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
			}
			if (cmd == "PASV")
			{
				if (data_pasv)
				{
					close(data_fd);
				}
				data_actv = false;
				data_fd = socket(AF_INET, SOCK_STREAM, 0);
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
					cout << "Error!\n";
					bye();
				}
				socklen_t len = sizeof(sockaddr);
				if (getsockname(data_fd, (sockaddr*)&cldata, &len) == -1)
				{
					close(data_fd);
					cout << "Error!\n";
					bye();
				}
				int port = ntohs(cldata.sin_port);
				char ip[16];
				get_local_ip(ip);
				string sip = ip, d = ".";
				vector<string> sp = split(sip, d);
				string res = "227 Entering Passive Mode (" + sp[0] + "," + sp[1] + "," + sp[2] + "," + sp[3] + "," + to_string(port / 256) + "," + to_string(port % 256) + ").\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				data_pasv = true;
			}
			if (cmd == "PORT")
			{
				if (data_pasv)
				{
					close(data_fd);
					data_pasv = false;
				}
				string d = ",";
				vector<string> sp = split(para, d);
				int port = 256 * stoi(sp[4]) + stoi(sp[5]);
				memset(&data_addr, 0, sizeof(sockaddr_in));
				data_addr.sin_addr.s_addr = cl_addr.sin_addr.s_addr;
				data_addr.sin_family = AF_INET;
				data_addr.sin_port = htons(port);
				string res = "200 PORT command successful. Consider using PASV.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				data_actv = true;
			}
			if (cmd == "PWD" || cmd == "XPWD")
			{
				strcpy(buf, "257 \"");
				char path[255];
				getcwd(path, 255);
				strcat(buf, path);
				strcat(buf, "\" is the current directory\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "QUIT")
			{
				strcpy(buf, "221 Goodbye.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
				bye();
			}
			if (cmd == "REIN")
			{
				username = "";
				login = false;
				strcpy(buf, "220 Service ready for new user.\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "REST")
			{
				offset = (para.size() && is_num(para)) ? stoi(para) : 0;
				strcpy(buf, "350 Restart position accepted (");
				strcat(buf, to_string(offset).c_str());
				strcat(buf, ").\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "RETR")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					offset = 0;
					continue;
				}
				ifstream in(para);
				struct stat st;
				stat(para.c_str(), &st);
				if (!in.is_open() || S_ISDIR(st.st_mode))
				{
					res = "550 Failed to open file.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					offset = 0;
					close(data_fd);
					continue;
				}
				if (offset < st.st_size)
				{
					if (data_actv)
					{
						cnct();
					}
					else
					{
						acpt();
					}
					res = "150 Opening BINARY mode data connection for " + para + " (" + to_string(st.st_size) + " bytes).\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					in.seekg(offset, ios::beg);
					for (int i = st.st_size - offset; i > 0; i -= min(i, BUFSIZE))
					{
						in.read(buf, min(i, BUFSIZE));
						while (send(data_fd, buf, min(i, BUFSIZE), 0) == -1);
					}
				}
				else
				{
					res = "150 Opening BINARY mode data connection for " + para + " (" + to_string(st.st_size) + " bytes).\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				}
				res = "226 Transfer complete.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				close(data_fd);
				offset = 0;
				in.close();
			}
			if (cmd == "RMD" || cmd == "XRMD")
			{
				if (rmdir(para.c_str()) == -1)
				{
					strcpy(buf, "550 Remove directory operation failed.\r\n");
				}
				else
				{
					strcpy(buf, "250 Remove directory operation successful.\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "RNFR")
			{
				struct stat st;
				int res = stat(para.c_str(), &st);
				if (res == -1)
				{
					strcpy(buf, "550 RNFR command failed.\r\n");
				}
				else
				{
					fr_name = para;
					strcpy(buf, "350 Ready for RNTO.\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "RNTO")
			{
				if (fr_name == "")
				{
					strcpy(buf, "503 RNFR required first.\r\n");
				}
				else
				{
					if (rename(fr_name.c_str(), para.c_str()) == -1)
					{
						strcpy(buf, "550 Rename failed.\r\n");
					}
					else
					{
						strcpy(buf, "250 Rename successful.\r\n");
					}
					fr_name = "";
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "SIZE")
			{
				struct stat st;
				int res = stat(para.c_str(), &st);
				if (res == -1 || S_ISDIR(st.st_mode))
				{
					strcpy(buf, "550 Could not get file size.\r\n");
				}
				else
				{
					strcpy(buf, "213 ");
					strcat(buf, to_string(st.st_size).c_str());
					strcat(buf, "\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "STAT")
			{
				string res;
				if (para == "")
				{
					res = "211-FTP server status:\r\n";
					res += "    Connected to ::ffff:";
					string cl_ip = inet_ntoa(cl_addr.sin_addr);
					res += cl_ip;
					res += "\r\n    Logged in as ";
					res += username;
					res += "\r\n    TYPE: ";
					res += (bi_mode) ? "binary" : "ascii";
					res += "\r\n    No session bandwidth limit\r\n    Session timeout in seconds is 300\r\n    Control connection is plain text\r\n    Data connections will be plain text\r\n    At session startup, client count was ";
					res += to_string(user_cnt);
					res += "\r\n    ylxFTP 1.0.0 - secure, fast, stable\r\n211 End of status\r\n";
				}
				else
				{
					res = "213-Status follows:\r\n";
					DIR* pDir;
					struct dirent* pEnt;
					pDir = opendir(para.c_str());
					string newname;
					while (pEnt = readdir(pDir))
					{
						newname = pEnt->d_name;
						newname = para + ((para.back() == '/') ? "" : "/") + newname;
						res += get_detailed_info(newname) + "\r\n";
					}
					res += "213 End of status\r\n";
				}
				while ((send(con_fd, res.c_str(), res.size(), 0)) == -1);
			}
			if (cmd == "STOR")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					offset = 0;
					continue;
				}
				fstream out;
				if (offset)
				{
					out.open(para, ios::binary | ios::out | ios::in);
					out.seekp(offset, ios::beg);
				}
				else
				{
					out.open(para, ios::binary | ios::out);
				}
				if (data_actv)
				{
					cnct();
				}
				else
				{
					acpt();
				}
				res = "150 Ok to send data.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				int num;
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					out.write(buf, num);
				}
				res = "226 Transfer complete.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				close(data_fd);
				offset = 0;
				out.close();
			}
			if (cmd == "STOU")
			{
				string res;
				if (!data_actv && !data_pasv)
				{
					res = "425 Use PORT or PASV first.\r\n";
					while (send(con_fd, res.c_str(), res.size(), 0) == -1);
					offset = 0;
					continue;
				}
				ofstream out;
				string name = para;
				struct stat st;
				if (stat(para.c_str(), &st) != -1)
				{
					int i = 1;
					while (true)
					{
						if (stat((para + "." + to_string(i)).c_str(), &st) == -1)
						{
							name = para + "." + to_string(i);
							break;
						}
						++i;
					}
				}
				out.open(name, ios::binary);
				if (data_actv)
				{
					cnct();
				}
				else
				{
					acpt();
				}
				res = "150 FILE: " + name + "\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				int num;
				while ((num = recv(data_fd, buf, BUFSIZE, 0)) > 0)
				{
					buf[num] = 0;
					out.write(buf, num);
				}
				res = "226 Transfer complete.\r\n";
				while (send(con_fd, res.c_str(), res.size(), 0) == -1);
				close(data_fd);
				offset = 0;
				out.close();
			}
			if (cmd == "SYST")
			{
				strcpy(buf, "215 UNIX Type: L8\r\n");
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "TYPE")
			{
				if (para != "I" && para != "i" && para != "A" && para != "a")
				{
					strcpy(buf, "500 Unrecognised TYPE command.\r\n");
				}
				else
				{
					if (para == "I" || para == "i")
					{
						bi_mode = true;
						strcpy(buf, "200 Switching to Binary mode.\r\n");
					}
					else
					{
						bi_mode = false;
						strcpy(buf, "200 Switching to ASCII mode.\r\n");
					}
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
			if (cmd == "USER")
			{
				if (login)
				{
					if (para == username)
					{
						strcpy(buf, "331 Any password will do.\r\n");
					}
					else
					{
						strcpy(buf, "530 Can't change to another user.\r\n");
					}
				}
				else
				{
					username = para;
					strcpy(buf, "331 Please specify the password.\r\n");
				}
				while (send(con_fd, buf, strlen(buf), 0) == -1);
			}
		}		
	}

	~server()
	{
		close(con_fd);
		close(data_fd);
	}
};


int main(int argc, char* argv[])
{
	if (argc != 4)
	{
		cout << "Usage: ./FILENAME NICname Username Password\n";
		return 1;
	}
	eth = argv[1];
	un = argv[2];
	pw = argv[3];
	signal(16, sigfun);
	signal(2, sigfun);
	signal(14, sigfun);
	signal(3, sigfun);
	sockaddr_in ser_addr, cl_addr;
	memset(&ser_addr, 0, sizeof(sockaddr_in));
	ser_addr.sin_addr.s_addr = INADDR_ANY;
	ser_addr.sin_port = htons(21);
	ser_addr.sin_family = AF_INET;
	int con_fd;
	while ((lis_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1);
	int opt = 1;
	setsockopt(lis_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if ((bind(lis_fd, (sockaddr*)&ser_addr, sizeof(sockaddr))) == -1)
	{
		cout << "Cannot bind to port 21.\nThe possible reason is the program doesn't have administrator privileges.\n";
		close(lis_fd);
		return 1;
	}
	if ((listen(lis_fd, BACKLOG)) == -1)
	{
		cout << "An error occurs while listening.\n";
		close(lis_fd);
		return 1;
	}
	while (true)
	{
		memset(&cl_addr, 0, sizeof(sockaddr_in));
		socklen_t sin_size = sizeof(sockaddr_in);
		con_fd = accept(lis_fd, (sockaddr*)&cl_addr, &sin_size);
		if (con_fd == -1)
		{
			cout << "An error occurs while accepting.\n";
			close(lis_fd);
			return 1;
		}
		++user_cnt;
		if (!fork())
		{
			server se(con_fd, cl_addr);
			se.ser_main();
			return 0;
		}
		close(con_fd);
	}
	return 0;
}