#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sstream>  
#include <iostream>
#include <random>
#include <iostream>
#include <fstream>
#include <random>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include "json.hpp"

using namespace std;
using json = nlohmann::json;




// 设备码存储路径
const string imeiPath = "/sdcard/.imei";  // 存储设备码的文件路径

// 生成一个伪 UUID
string generateUUID() {
    random_device rd;
    mt19937 gen(rd());  // 使用随机数生成器
    uniform_int_distribution<int> dist(0, 10);  // 十六进制数字

    stringstream ss;

    // 按照UUID格式生成随机值
    for (int i = 0; i < 4; ++i) {
        ss << hex << dist(gen);  // 生成4个随机数字
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
        ss << hex << dist(gen);  // 生成4个随机数字
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
        ss << hex << dist(gen);  // 生成4个随机数字
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
        ss << hex << dist(gen);  // 生成4个随机数字
    }
    ss << "-";
    for (int i = 0; i < 4; ++i) {
        ss << hex << dist(gen);  // 生成4个随机数字
    }

    return ss.str();
}

// 读取IMEI从文件
string readIMEIFromFile(const string& path) {
    ifstream file(path);
    if (file.is_open()) {
        string imei;
        getline(file, imei);  // 读取文件中的内容
        file.close();
        return imei;
    }
    return "";  // 文件为空或无法打开
}

// 保存IMEI到文件
void saveIMEIToFile(const string& path, const string& imei) {
    ofstream file(path);
    if (file.is_open()) {
        file << imei << endl;  // 将IMEI写入文件
        file.close();
    } else {
        cerr << "无法保存IMEI到文件: " << path << endl;
    }
}

// 创建目录
bool createDirectory(const string& path) {
    // 检查路径是否存在
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        // 如果目录不存在，尝试创建它
        if (mkdir(path.c_str(), 0777) == -1) {
            cerr << "无法创建目录: " << path << endl;
            return false;
        }
    }
    return true;
}

// 获取IMEI（如果文件存在则读取，否则生成并保存）
string getIMEI() {
    // 获取目录部分
    size_t lastSlashPos = imeiPath.find_last_of('/');
    string dirPath = imeiPath.substr(0, lastSlashPos);

    // 检查并创建目录
    if (!createDirectory(dirPath)) {
        return "";  // 如果创建目录失败，则返回空字符串
    }

    // 读取现有的IMEI值
    string imei = readIMEIFromFile(imeiPath);
    if (imei.empty()) {
        // 如果文件为空或文件不存在，生成新的UUID
        imei = generateUUID();
        saveIMEIToFile(imeiPath, imei);  // 保存新的IMEI到文件
    }

    return imei;  // 返回读取或生成的IMEI
}

string strstrstr(const string& str, const string& front, const string& rear) {
    size_t front_pos = str.find(front);
    if (front_pos == string::npos) {
        return "";
    }

    size_t rear_pos = str.find(rear, front_pos + front.length());
    if (rear_pos == string::npos) {
        return "";
    }

    return str.substr(front_pos + front.length(), rear_pos - front_pos - front.length());
}


int hextoint(const string& hex) {
    int value = 0;
    for (char c : hex) {
        if (c >= 'A' && c <= 'F') {
            value = (c - 'A' + 10) + 16 * value;
        } else if (c >= 'a' && c <= 'f') {
            value = (c - 'a' + 10) + 16 * value;
        } else if (c >= '0' && c <= '9') {
            value = (c - '0') + 16 * value;
        } else {
            return value;
        }
    }
    return value;
}

vector<unsigned char> build_dns_query(const string& domain, uint16_t id) {
    vector<unsigned char> packet;

    // 事务ID（大端序）
    packet.push_back((id >> 8) & 0xFF);
    packet.push_back(id & 0xFF);

    // 标志：标准查询，递归请求
    packet.push_back(0x01); // Flags: 0x0100
    packet.push_back(0x00);

    // 问题数：1
    packet.push_back(0x00);
    packet.push_back(0x01);

    // 回答、授权、额外记录数均为0
    for (int i = 0; i < 6; ++i) {
        packet.push_back(0x00);
    }

    // 域名处理
    size_t start = 0;
    while (true) {
        size_t dot = domain.find('.', start);
        if (dot == string::npos) {
            string segment = domain.substr(start);
            packet.push_back(segment.length());
            for (char c : segment) {
                packet.push_back(c);
            }
            break;
        }
        string segment = domain.substr(start, dot - start);
        packet.push_back(segment.length());
        for (char c : segment) {
            packet.push_back(c);
        }
        start = dot + 1;
    }
    packet.push_back(0x00); // 结束域名

    // 查询类型A，类IN
    packet.push_back(0x00);
    packet.push_back(0x01); // Type A
    packet.push_back(0x00);
    packet.push_back(0x01); // Class IN

    return packet;
}

string parse_dns_response(const vector<unsigned char>& data, uint16_t expected_id) {
    if (data.size() < 12) return "";

    // 检查事务ID
    uint16_t id = (data[0] << 8) | data[1];
    if (id != expected_id) return "";

    uint16_t ancount = (data[6] << 8) | data[7];
    if (ancount == 0) return "";

    // 跳过问题部分
    size_t pos = 12;
    while (pos < data.size() && data[pos] != 0) {
        pos += data[pos] + 1;
    }
    pos += 5; // 跳过末尾0和QTYPE/QCLASS

    // 遍历回答记录
    for (int i = 0; i < ancount && pos < data.size(); ++i) {
        // 跳过域名（可能是指针）
        if ((data[pos] & 0xC0) == 0xC0) {
            pos += 2;
        } else {
            while (pos < data.size() && data[pos] != 0) {
                pos += data[pos] + 1;
            }
            pos++;
        }

        if (pos + 10 > data.size()) break;

        uint16_t type = (data[pos] << 8) | data[pos + 1];
        pos += 2; // 跳过TYPE
        pos += 2; // 跳过CLASS
        pos += 4; // 跳过TTL
        uint16_t rdlength = (data[pos] << 8) | data[pos + 1];
        pos += 2;

        if (type == 0x0001 && rdlength == 4) { // A记录
            if (pos + 4 > data.size()) break;
            char ip[16];
            sprintf(ip, "%d.%d.%d.%d", data[pos], data[pos+1], data[pos+2], data[pos+3]);
            return string(ip);
        }
        pos += rdlength;
    }
    return "";
}

int read_fully(int sockfd, char* buffer, int length) {
    int total_read = 0;
    while (total_read < length) {
        int bytes_read = read(sockfd, buffer + total_read, length - total_read);
        if (bytes_read <= 0) return -1;
        total_read += bytes_read;
    }
    return total_read;
}

string resolve_with_aliyun(const string& domain) {
    srand(time(nullptr));
    uint16_t id = rand() % 0xFFFF;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return "";

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(53);
    if (inet_pton(AF_INET, "223.5.5.5", &serveraddr.sin_addr) <= 0) {
        close(sockfd);
        return "";
    }

    if (connect(sockfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        close(sockfd);
        return "";
    }

    // 构建DNS查询并发送
    vector<unsigned char> query = build_dns_query(domain, id);
    uint16_t query_len = query.size();
    vector<unsigned char> tcp_packet;
    tcp_packet.push_back((query_len >> 8) & 0xFF);
    tcp_packet.push_back(query_len & 0xFF);
    tcp_packet.insert(tcp_packet.end(), query.begin(), query.end());

    if (send(sockfd, tcp_packet.data(), tcp_packet.size(), 0) <= 0) {
        close(sockfd);
        return "";
    }

    // 读取响应长度
    char len_bytes[2];
    if (read_fully(sockfd, len_bytes, 2) != 2) {
        close(sockfd);
        return "";
    }
    uint16_t resp_len = (static_cast<uint8_t>(len_bytes[0]) << 8) | static_cast<uint8_t>(len_bytes[1]);

    // 读取响应数据
    vector<unsigned char> response(resp_len);
    if (read_fully(sockfd, reinterpret_cast<char*>(response.data()), resp_len) != resp_len) {
        close(sockfd);
        return "";
    }

    close(sockfd);
    return parse_dns_response(response, id);
}

string getip(const string& hostname) {
    string ip = resolve_with_aliyun(hostname);
    if (!ip.empty()) return ip;
    return NULL;
    /*struct hostent* host = gethostbyname(hostname.c_str());
    if (!host) return "";
    return inet_ntoa(*reinterpret_cast<struct in_addr*>(host->h_addr));*/
}

string read_chunk(int sockfd) {
    char chunk_len_hex[8] = {0};
    int len = 0;

    // 读取 chunk 长度（以 hex 格式）
    if (read(sockfd, chunk_len_hex, 1) <= 0) {
        return "";  // 错误读取 chunk 长度
    }

    while (chunk_len_hex[len] != '\n' && len < 7) {
        len++;
        if (read(sockfd, &chunk_len_hex[len], 1) <= 0) {
            return "";  // 错误读取 chunk 长度
        }
    }
    chunk_len_hex[len] = '\0';

    int chunk_size = hextoint(chunk_len_hex);
    if (chunk_size == 0) {
        return ""; // 所有 chunk 读取完毕
    }

    // 读取 chunk 数据
    vector<char> chunk_data(chunk_size);
    if (read_fully(sockfd, chunk_data.data(), chunk_size) != chunk_size) {
        std::cout << "错误代码4\n" << std::endl;
        return ""; // 错误读取 chunk 数据
    }

    // 跳过 "\r\n"
    char crlf[2];
    if (read_fully(sockfd, crlf, 2) != 2) {
        return ""; // 错误跳过 CRLF
    }

    return string(chunk_data.begin(), chunk_data.end());
}


string read_response(int sockfd) {
    vector<char> response(1024);
    char* ptr = response.data();
    char* max_ptr = response.data() + 1023;
    int readlen;

    // Read the response header
    while ((readlen = read(sockfd, ptr, 1)) > 0) {
        if (*ptr == '\n') {
            if (strncmp(ptr - 3, "\r\n\r", 3) == 0) {
                *(++ptr) = '\0';  // Null-terminate header
                break;
            }
        }
        ptr++;
        if (ptr == max_ptr) {
            return ""; // Error or oversized header
        }
    }

    if (readlen == 0) {
        return ""; // No data read
    }

    // Construct the header from the beginning of the response to the current pointer (ptr)
    string header(response.data(), ptr - response.data());

    // Get the content-length or start chunked transfer
    string content_length_str = strstrstr(header, "Content-Length: ", "\n");

    if (!content_length_str.empty()) {
        // Handle regular content-length response
        int content_length = stoi(content_length_str);
        vector<char> body_data(content_length);
        if (read(sockfd, body_data.data(), content_length) != content_length) {
            return ""; // Error reading the body
        }
        return string(body_data.begin(), body_data.end());
    } else {
        // Handle chunked transfer encoding
        string body;
        while (true) {
            string chunk = read_chunk(sockfd);
            if (chunk.empty()) {
                break;  // End of chunks or error
            }
            body += chunk;
        }
        return body;
    }
}


string httppost(const string& hostname, const string& url, const string& cs) {
    int sockfd;
    struct sockaddr_in serveraddr;
    socklen_t addrlen = sizeof(serveraddr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return "Error creating socket";
    }

    memset(&serveraddr, 0, addrlen);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(80);

    string ip = getip(hostname);
    if (ip.empty()) {
        close(sockfd);
        return "Host lookup failed";
    }

    inet_pton(AF_INET, ip.c_str(), &serveraddr.sin_addr);
    
    if (connect(sockfd, (struct sockaddr*)&serveraddr, addrlen) < 0) {
        close(sockfd);
        return "Connection failed";
    }

    stringstream postxyt;
    postxyt << "POST /" << url << " HTTP/1.1\r\n"
            << "Host: " << hostname << "\r\n"
            << "Content-Type: application/x-www-form-urlencoded\r\n"
            << "User-Agent: Mozilla/4.0(compatible)\r\n"
            << "Content-Length: " << cs.length() << "\r\n"
            << "\r\n" << cs << "\r\n\r\n";

    if (send(sockfd, postxyt.str().c_str(), postxyt.str().length(), 0) == -1) {
        close(sockfd);
        return "Error sending request";
    }

    string body = read_response(sockfd);

    close(sockfd);
    return body.empty() ? "No response or error" : body;
}

string httpget(const string& hostname, const string& url) {
    int sockfd;
    struct sockaddr_in serveraddr;
    socklen_t addrlen = sizeof(serveraddr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        return "Error creating socket";
    }

    memset(&serveraddr, 0, addrlen);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(80);

    struct hostent* host = gethostbyname(hostname.c_str());
    if (host == nullptr) {
        close(sockfd);
        return "Host lookup failed";
    }

    struct in_addr ip = *((struct in_addr*) host->h_addr);
    serveraddr.sin_addr = ip;

    if (connect(sockfd, (struct sockaddr*)&serveraddr, addrlen) < 0) {
        close(sockfd);
        return "Connection failed";
    }

    stringstream getxyt;
    getxyt << "GET /" << url << " HTTP/1.1\r\n"
            << "Host: " << hostname << "\r\n"
            << "Content-Type: application/x-www-form-urlencoded\r\n"
            << "\r\n";

    if (send(sockfd, getxyt.str().c_str(), getxyt.str().length(), 0) == -1) {
        close(sockfd);
        return "Error sending request";
    }

    string body = read_response(sockfd);

    close(sockfd);
    return body.empty() ? "No response or error" : body;
}


// md5
typedef struct
{
	unsigned int count[2];
	unsigned int state[4];
	unsigned char buffer[64];
} MD5_CTX;

#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))
#define ROTATE_LEFT(x, n) ((x << n) | (x >> (32 - n)))
#define FF(a, b, c, d, x, s, ac) \
  {                              \
    a += F(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define GG(a, b, c, d, x, s, ac) \
  {                              \
    a += G(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define HH(a, b, c, d, x, s, ac) \
  {                              \
    a += H(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#define II(a, b, c, d, x, s, ac) \
  {                              \
    a += I(b, c, d) + x + ac;    \
    a = ROTATE_LEFT(a, s);       \
    a += b;                      \
  }
#ifdef __cplusplus
extern "C"
{
#endif
	void MD5Init(MD5_CTX * context);
	void MD5Update(MD5_CTX * context, unsigned char *input, unsigned int inputlen);
	void MD5Final(MD5_CTX * context, unsigned char digest[16]);
	void MD5Transform(unsigned int state[4], unsigned char block[64]);
	void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len);
	void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len);
	
#ifdef __cplusplus
}
#endif

// md5代码区
unsigned char PADDING[] = { 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// 定制md5
void MD5Init(MD5_CTX * context)
{
	context->count[0] = 0;
	context->count[1] = 0;
	context->state[0] = 0x67452301;
	context->state[1] = 0xEFCDAB89;
	context->state[2] = 0x98BADCFE;
	context->state[3] = 0x10325476;
}

void MD5Update(MD5_CTX * context, unsigned char *input, unsigned int inputlen)
{
	unsigned int i = 0, index = 0, partlen = 0;
	index = (context->count[0] >> 3) & 0x3F;
	partlen = 64 - index;
	context->count[0] += inputlen << 3;
	if (context->count[0] < (inputlen << 3))
		context->count[1]++;
	context->count[1] += inputlen >> 29;

	if (inputlen >= partlen)
	{
		memcpy(&context->buffer[index], input, partlen);
		MD5Transform(context->state, context->buffer);
		for (i = partlen; i + 64 <= inputlen; i += 64)
			MD5Transform(context->state, &input[i]);
		index = 0;
	}
	else
	{
		i = 0;
	}
	memcpy(&context->buffer[index], &input[i], inputlen - i);
}

void MD5Final(MD5_CTX * context, unsigned char digest[16])
{
	unsigned int index = 0, padlen = 0;
	unsigned char bits[8];
	index = (context->count[0] >> 3) & 0x3F;
	padlen = (index < 56) ? (56 - index) : (120 - index);
	MD5Encode(bits, context->count, 8);
	MD5Update(context, PADDING, padlen);
	MD5Update(context, bits, 8);
	MD5Encode(digest, context->state, 16);
}

void MD5Encode(unsigned char *output, unsigned int *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[j] = input[i] & 0xFF;
		output[j + 1] = (input[i] >> 8) & 0xFF;
		output[j + 2] = (input[i] >> 16) & 0xFF;
		output[j + 3] = (input[i] >> 24) & 0xFF;
		i++;
		j += 4;
	}
}

void MD5Decode(unsigned int *output, unsigned char *input, unsigned int len)
{
	unsigned int i = 0, j = 0;
	while (j < len)
	{
		output[i] = (input[j]) | (input[j + 1] << 8) | (input[j + 2] << 16) | (input[j + 3] << 24);
		i++;
		j += 4;
	}
}

void MD5Transform(unsigned int state[4], unsigned char block[64])
{
	unsigned int a = state[0];
	unsigned int b = state[1];
	unsigned int c = state[2];
	unsigned int d = state[3];
	unsigned int x[64];
	MD5Decode(x, block, 64);
	FF(a, b, c, d, x[0], 7, 0xd76aa478);
	FF(d, a, b, c, x[1], 12, 0xe8c7b756);
	FF(c, d, a, b, x[2], 17, 0x242070db);
	FF(b, c, d, a, x[3], 22, 0xc1bdceee);
	FF(a, b, c, d, x[4], 7, 0xf57c0faf);
	FF(d, a, b, c, x[5], 12, 0x4787c62a);
	FF(c, d, a, b, x[6], 17, 0xa8304613);
	FF(b, c, d, a, x[7], 22, 0xfd469501);
	FF(a, b, c, d, x[8], 7, 0x698098d8);
	FF(d, a, b, c, x[9], 12, 0x8b44f7af);
	FF(c, d, a, b, x[10], 17, 0xffff5bb1);
	FF(b, c, d, a, x[11], 22, 0x895cd7be);
	FF(a, b, c, d, x[12], 7, 0x6b901122);
	FF(d, a, b, c, x[13], 12, 0xfd987193);
	FF(c, d, a, b, x[14], 17, 0xa679438e);
	FF(b, c, d, a, x[15], 22, 0x49b40821);

	GG(a, b, c, d, x[1], 5, 0xf61e2562);
	GG(d, a, b, c, x[6], 9, 0xc040b340);
	GG(c, d, a, b, x[11], 14, 0x265e5a51);
	GG(b, c, d, a, x[0], 20, 0xe9b6c7aa);
	GG(a, b, c, d, x[5], 5, 0xd62f105d);
	GG(d, a, b, c, x[10], 9, 0x2441453);
	GG(c, d, a, b, x[15], 14, 0xd8a1e681);
	GG(b, c, d, a, x[4], 20, 0xe7d3fbc8);
	GG(a, b, c, d, x[9], 5, 0x21e1cde6);
	GG(d, a, b, c, x[14], 9, 0xc33707d6);
	GG(c, d, a, b, x[3], 14, 0xf4d50d87);
	GG(b, c, d, a, x[8], 20, 0x455a14ed);
	GG(a, b, c, d, x[13], 5, 0xa9e3e905);
	GG(d, a, b, c, x[2], 9, 0xfcefa3f8);
	GG(c, d, a, b, x[7], 14, 0x676f02d9);
	GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

	HH(a, b, c, d, x[5], 4, 0xfffa3942);
	HH(d, a, b, c, x[8], 11, 0x8771f681);
	HH(c, d, a, b, x[11], 16, 0x6d9d6122);
	HH(b, c, d, a, x[14], 23, 0xfde5380c);
	HH(a, b, c, d, x[1], 4, 0xa4beea44);
	HH(d, a, b, c, x[4], 11, 0x4bdecfa9);
	HH(c, d, a, b, x[7], 16, 0xf6bb4b60);
	HH(b, c, d, a, x[10], 23, 0xbebfbc70);
	HH(a, b, c, d, x[13], 4, 0x289b7ec6);
	HH(d, a, b, c, x[0], 11, 0xeaa127fa);
	HH(c, d, a, b, x[3], 16, 0xd4ef3085);
	HH(b, c, d, a, x[6], 23, 0x4881d05);
	HH(a, b, c, d, x[9], 4, 0xd9d4d039);
	HH(d, a, b, c, x[12], 11, 0xe6db99e5);
	HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
	HH(b, c, d, a, x[2], 23, 0xc4ac5665);

	II(a, b, c, d, x[0], 6, 0xf4292244);
	II(d, a, b, c, x[7], 10, 0x432aff97);
	II(c, d, a, b, x[14], 15, 0xab9423a7);
	II(b, c, d, a, x[5], 21, 0xfc93a039);
	II(a, b, c, d, x[12], 6, 0x655b59c3);
	II(d, a, b, c, x[3], 10, 0x8f0ccc92);
	II(c, d, a, b, x[10], 15, 0xffeff47d);
	II(b, c, d, a, x[1], 21, 0x85845dd1);
	II(a, b, c, d, x[8], 6, 0x6fa87e4f);
	II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
	II(c, d, a, b, x[6], 15, 0xa3014314);
	II(b, c, d, a, x[13], 21, 0x4e0811a1);
	II(a, b, c, d, x[4], 6, 0xf7537e82);
	II(d, a, b, c, x[11], 10, 0xbd3af235);
	II(c, d, a, b, x[2], 15, 0x2ad7d2bb);
	II(b, c, d, a, x[9], 21, 0xeb86d391);
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
}

// Function to compute MD5 hash of a given string
std::string kd075af07a001725ff37354555d4d6b1e(const std::string& inputStr) {
    static char _SignMd5[33]; // Static so it persists after the function returns
    unsigned char _Decrypt[16]; // MD5 output is 16 bytes
    MD5_CTX md5c;
    
    // Initialize MD5 context
    MD5Init(&md5c);
    
    // Update MD5 context with input string
    MD5Update(&md5c, (unsigned char *)inputStr.c_str(), inputStr.length());
    
    // Finalize the MD5 hash calculation
    MD5Final(&md5c, _Decrypt);
    
    // Convert the MD5 result into a hexadecimal string
    for (int i = 0; i < 16; i++) {
        sprintf(&_SignMd5[i * 2], "%02x", _Decrypt[i]);
    }

    return std::string(_SignMd5); // Return the MD5 hash as a string
}

string d41f4f21e10151e76595b862b27e1bce6(const string& hex) {
    stringstream ss;
    string bin;
    for (size_t i = 0; i < hex.length(); i += 2) {
        string byte = hex.substr(i, 2);
        char chr = static_cast<char>(stoi(byte, nullptr, 16));
        bin += chr;
    }
    return bin;
}

string i33196c91697f29f11b49ce0f817e4b58(const string& bin) {
    stringstream ss;
    for (size_t i = 0; i < bin.size(); ++i) {
        ss << hex << setw(2) << setfill('0') << (int)(unsigned char)bin[i];
    }
    return ss.str();
}

string mbba511f0a321bc03f52ee539077c6370(const string& text, const string& key) {
    vector<int> s(256);  // Use a vector of integers to represent the state array
    int j = 0;
    for (int i = 0; i < 256; i++) {
        s[i] = i;
    }

    // Key scheduling algorithm
    for (int i = 0; i < 256; i++) {
        j = (j + s[i] + key[i % key.length()]) % 256;
        swap(s[i], s[j]);
    }

    // Pseudo-random generation algorithm
    string ciphertext;
    int i = 0, k = 0;
    for (size_t n = 0; n < text.length(); n++) {
        i = (i + 1) % 256;
        k = (k + s[i]) % 256;
        swap(s[i], s[k]);
        int t = (s[i] + s[k]) % 256;
        ciphertext += static_cast<char>(text[n] ^ s[t]);
    }

    return ciphertext;
}


static const string base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// Function to encode a string to Base64
string sdf0c5c6329f62210f82c2b244c3fd0eb(const string& input) {
    string encoded_string;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded_string.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded_string.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded_string.size() % 4) {
        encoded_string.push_back('=');
    }
    return encoded_string;
}

// Function to decode a Base64 encoded string
string c6629f42601a27d6ccf7329df35c570bc(const string& encoded_string) {
    vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    int val = 0, valb = -8;
    string decoded_string;
    for (unsigned char c : encoded_string) {
        if (T[c] == -1) break; // Skip non-Base64 characters (such as '=')
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded_string.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded_string;
}

string ga3a64c47d85982908b78434105b1e710(const string& input,const string& base64_chars) {
    string encoded_string;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded_string.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        encoded_string.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (encoded_string.size() % 4) {
        encoded_string.push_back('=');
    }
    return encoded_string;
}

// Function to decode a Base64 encoded string
string y53015a4e0ea5b8b3238d94acd9288308(const string& encoded_string, const string base64_chars) {
    vector<int> T(256, -1);
    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }
    int val = 0, valb = -8;
    string decoded_string;
    for (unsigned char c : encoded_string) {
        if (T[c] == -1) break; // Skip non-Base64 characters (such as '=')
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded_string.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return decoded_string;
}



