/*
 * =====================================================================================
 *
 *       Filename:  Server.cpp
 *
 *    Description:  Server
 *
 *        Version:  1.0
 *        Created:  10/11/2015 02:51:33 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  huangyun (hy), 895175589@qq.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "Server.h"
#include "MyClientTaskManager.h"
#include "MyClientTask.h"
#include "MySockTaskManager.h"
#include "MyServerMsgProcess.h"
#include "MyServerTask.h"
#include "MyThread.h"
#include "XmlconfigParse.h"
#include "CmdNumber.h"
#include "MyCsvParse.h"

namespace MyNameSpace
{
	namespace
	{
		class ServerMsgProcess : public MyThread, public MySingleton<ServerMsgProcess>
		{
			private:
				friend class MySingleton<ServerMsgProcess>;
				ServerMsgProcess()
				{

				}
				~ServerMsgProcess()
				{

				}
			public:
				void run()
				{
					while (!isFini())
					{
						MySockTaskManager::getInstance().doProcessMsg();
						MyClientTaskManager::getInstance().doProcessMsg();
						usleep(5*1000);	
					}
				}
		};
	}

	bool Server::init(unsigned short port)
	{
		if (!mServerTaskPool.init())//new recycle and IO thread
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): mServerTaskPool init fail"<<std::endl;
			return false;
		}
		if (!MyBaseServer::init(port))//init socket related settings and epoll settings
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): MyBaseServer init fail"<<std::endl;
			return false;
		}
		if (!mClientTaskPool.init())//new recycle and IO thread, and start them£¬ use thread to process send and recv, not detail message
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): mClientTaskPool init fail"<<std::endl;
			return false;
			return false;
		}
 		if (!ServerMsgProcess::getInstance().start())//start message process thread, MySockTaskManager and MyClientTaskManager instance call doProcessMsg to handle
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): ServerMsgProcess start fail"<<std::endl;
			return false;
		}

		//load clinet info from xml
		XmlConfig::loadClientConfig("configure/clientAddress.xml", clientInfoList);
		for (auto iter : clientInfoList)
		{
			std::cout<<"ip: "<<iter.ip<<" id: "<<iter.id<<" port: "<<iter.port<<" type: "<<iter.type<<std::endl;
			newClient(iter.ip.c_str(), iter.port, iter.id, iter.type);//new client through new myclienttask
		}

		if (!MyCsvParse::getInstance().init())
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): mCsvParse init fail"<<std::endl;
			return false;
		}
		initCallBack();
		return true;

	}
	bool Server::reload()
	{
		//TODO reload config
		return true;
	}
	void Server::initCallBack()
	{
		regOutterCallBack(Command::OutterCommand::REQ_LOGIN_CMD, std::bind(&LoginProcess::ReqLogin, &loginProcess, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
		regOutterCallBack(Command::OutterCommand::TEST_PROTO_BUF, std::bind(&LoginProcess::testProtobuf, &loginProcess, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	}
	bool Server::newTask(int sock)
	{
//		std::cout<<__FUNCTION__<<"("<<__LINE__<<"): new task"<<std::endl;
		std::cout<<"new task, id :"<<mServerUniqueId<<std::endl;

		MyServerTask *task = new MyServerTask(sock, mServerUniqueId, &mInnerDispatcher, &mOutterDispatcher);
		if (NULL == task)
		{
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): new task fail"<<std::endl;
			return false;
		}
		MySockTaskManager::getInstance().addTask(task);
		if (!mServerTaskPool.addTask(task))
		{
			MySockTaskManager::getInstance().removeTask(task);
			delete task;
			std::cerr<<__FUNCTION__<<"("<<__LINE__<<"): add task fail"<<std::endl;
			return false;
		}
		++mServerUniqueId;
		return true;
	}

	bool Server::newClient(const char* ip, unsigned short port ,int serverId, int serverType)
	{
		MyClientTask *task = new MyClientTask(mClientUniqueId, inet_addr(ip), port, serverId, serverType, &mInnerDispatcher, &mOutterDispatcher);
		if (NULL != task)
		{
			++mClientUniqueId;
			MyClientTaskManager::getInstance().addTask(task);
			mClientTaskPool.addTask(task);//rycycle and io thread will handle this task in bakc-end
			return true;
		}
		else
		{
			return false;
		}
	}

	void Server::fini()
	{
		MyBaseServer::fini();
		mServerTaskPool.fini();
		mClientTaskPool.fini();
		ServerMsgProcess::getInstance().terminate();
	}
}
