/////////////////////////////////////////////////////////////////////////
// ServerPrototype.cpp - Console App that processes incoming messages  //
// ver 1.0                                                             //
// Jim Fawcett, CSE687 - Object Oriented Design, Spring 2018           //
// ver 2.0                                                             //
// Datan Xu,    CSE687 - Object Oriented Design, Spring 2018           //
/////////////////////////////////////////////////////////////////////////

#include "ServerPrototype.h"
#include "../FileSystem-Windows/FileSystemDemo/FileSystem.h"
#include <chrono>
#include "../Datan-project2/RepositoryCore/RepositoryCore.h"
#include <fstream>
#include <sstream>

namespace MsgPassComm = MsgPassingCommunication;

using namespace Repository;
using namespace FileSystem;
using Msg = MsgPassingCommunication::Message;

NoSqlDb::DbCore<PayLoad> db;
Repository::RepositoryCore repoCore(db);

Files Server::getFiles(const Repository::SearchPath& path)
{
  return Directory::getFiles(path);
}

Dirs Server::getDirs(const Repository::SearchPath& path)
{
  return Directory::getDirectories(path);
}

template<typename T>
void show(const T& t, const std::string& msg)
{
  std::cout << "\n  " << msg.c_str();
  for (auto item : t)
  {
    std::cout << "\n    " << item.c_str();
  }
}

//--------< Message process for "echo" command >--------------

std::function<Msg(Msg)> echo = [](Msg msg) {
  Msg reply = msg;
  reply.to(msg.from());
  reply.from(msg.to());
  return reply;
};

//--------< Message process for "getFiles" command >--------------

std::function<Msg(Msg)> getFiles = [](Msg msg) {
  Msg reply;
  reply.to(msg.from());
  reply.from(msg.to());
  reply.command("getFiles");
  std::string path = msg.value("path");
  if (path != "")
  {
    std::string searchPath = storageRoot;
    if (path != ".")
      searchPath = searchPath + "\\" + path;
    Files files = Server::getFiles(searchPath);
    size_t count = 0;
    for (auto item : files)
    {
      std::string countStr = Utilities::Converter<size_t>::toString(++count);
      reply.attribute("file" + countStr, item);
    }
  }
  else
  {
    std::cout << "\n  getFiles message did not define a path attribute";
  }
  return reply;
};

//--------< Message process for "getDirs" command >---------------

std::function<Msg(Msg)> getDirs = [](Msg msg) {
  Msg reply;
  reply.to(msg.from());
  reply.from(msg.to());
  reply.command("getDirs");
  std::string path = msg.value("path");
  if (path != "")
  {
    std::string searchPath = storageRoot;
    if (path != ".")
      searchPath = searchPath + "\\" + path;
    Files dirs = Server::getDirs(searchPath);
    size_t count = 0;
    for (auto item : dirs)
    {
      if (item != ".." && item != ".")
      {
        std::string countStr = Utilities::Converter<size_t>::toString(++count);
        reply.attribute("dir" + countStr, item);
      }
    }
  }
  else
  {
    std::cout << "\n  getDirs message did not define a path attribute";
  }
  return reply;
};

//--------< Message process for "checkIn" command >---------------

std::function<Msg(Msg)> checkIn = [](Msg msg) {
	NoSqlDb::DbElement<PayLoad> demoElem;
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reCheckIn");

	// build demoElem by the received message
	demoElem.name(msg.value("owner"));
	demoElem.descrip(msg.value("descript"));
	demoElem.dateTime(DateTime().now());
	demoElem.payLoad(PayLoad("../Storage/"));
	demoElem.nameSpace(msg.value("namespace"));

	PayLoad p = demoElem.payLoad();
	std::vector<std::string> cate;
	if (msg.value("cate") != "")
	{
		std::stringstream ssm(msg.value("cate"));
		std::string Cate;
		while (ssm >> Cate)
		{
			cate.push_back(Cate);
		}
	}
	p.categories(cate);
	demoElem.payLoad(p);

	if (msg.value("dependencies") != "")
	{
		std::stringstream ssm(msg.value("dependencies"));
		std::string child;
		while (ssm >> child)
		{
			demoElem.children().push_back(child);
		}
	}
	if (repoCore.checkIn(msg.value("file"), demoElem))
	{
		db = repoCore.dbCore();
		reply.attribute("flag", "y");
	}
	else {
		reply.attribute("flag", "n");
	}
	return reply;
};

//--------< Message process for "closeFile" command >---------------

std::function<Msg(Msg)> closeFile = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reCloseFile");
	

	if (db.contains(msg.value("closeFname")))
	{
		reply.attribute("flag","y");
		reply.attribute("closedFile", msg.value("closeFname"));
		PayLoad p = db[msg.value("closeFname")].payLoad();
		p.status(false);
		db[msg.value("closeFname")].payLoad(p);
		repoCore.dbCore(db);
	}
	else
	{
		reply.attribute("closedFile", msg.value("closeFname"));
		reply.attribute("flag", "n");
	}
	return reply;
};

//--------< Message process for "checkOut" command >---------------

std::function<Msg(Msg)> checkOut = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reCheckOut");

	std::size_t found = msg.value("fname").find_last_of(".");
	std::string fname = msg.value("fname").substr(0, found);
	std::string versionNum = msg.value("fname").substr(found);

	if (repoCore.checkOut(fname, versionNum))
	{
		std::vector<std::string> Parents = repoCore.dbCore()[fname].children();
		if (!Parents.empty())
		{
			std::string parents;
			for (auto i : Parents)
			{
				parents = parents + i + " ";
			}
			reply.attribute("parents", parents);
		}
		else
		{
			reply.attribute("parents", "");
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

		reply.attribute("file", fname);
		reply.attribute("srcPath", "../TempForServer/");
		reply.attribute("rcvPath", "../../../../ClientStr/Checked_Out_Files/");
		return reply;
	}
};

//--------< Message process for "checkOutParents" command >---------------

std::function<Msg(Msg)> checkOutParents = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reCheckOutParents");

	reply.attribute("file", msg.value("fname"));
	reply.attribute("srcPath", "../TempForServer/");
	reply.attribute("rcvPath", "../../../../ClientStr/Checked_Out_Files/");
	return reply;
};

std::function<Msg(Msg)> checkOut2 = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reCheckOut");

	std::size_t found = msg.value("fname").find_last_of(".");
	std::string fname = msg.value("fname").substr(0, found);
	reply.attribute("file", fname);
	reply.attribute("srcPath", "../Storage");
	reply.attribute("rcvPath", "../../../ClientStr2");
	return reply;
};

//--------< Message process for "query" command >--------------

std::function<Msg(Msg)> query = [](Msg msg) {
	Query<PayLoad> q(db);
	std::string category = msg.value("category");
	auto hasCategory = [&category](DbElement<PayLoad>& elem) {
		return (elem.payLoad()).hasCategory(category);
	};
	q.select(hasCategory).show();
	std::vector<std::string> keys = q.keys();
	std::string queryKeys;
	for (auto key : keys)
	{
		queryKeys += key + " ";
	}
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reQuery");
	reply.attribute("keys", queryKeys);
	return reply;
};
//--------< Message process for "browse" command >---------------

std::function<Msg(Msg)> browse = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reBrowse");
	reply.attribute("file", msg.value("fname"));
	reply.attribute("srcPath", "../Storage/");
	reply.attribute("rcvPath", "../../../../TempForClient/");	
	return reply;
};

std::function<Msg(Msg)> browse2 = [](Msg msg) {
	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reBrowse");
	reply.attribute("file", msg.value("fname"));
	reply.attribute("srcPath", "../Storage");
	reply.attribute("rcvPath", "../../../temp");
	return reply;
};

//--------< Message process for "metedata" command >---------------

std::function<Msg(Msg)> meteData = [](Msg msg) {
	NoSqlDb::DbElement<PayLoad> demoElem;
	std::string children;
	std::string cates;

	Msg reply;
	reply.to(msg.from());
	reply.from(msg.to());
	reply.command("reMetedata");

	std::size_t found = msg.value("fName").find_last_of(".");
	std::string fname = msg.value("fName").substr(0,found);
	demoElem = repoCore.dbCore()[fname];
	reply.attribute("description",demoElem.descrip());
	reply.attribute("namespace",demoElem.nameSpace());

	for (auto i : demoElem.payLoad().categories())
	{
		cates += i + " ";
	}
	reply.attribute("category", cates);
	reply.attribute("time", std::string(demoElem.dateTime()));

	for (auto i : demoElem.children())
	{
		children += i + " ";
	}

	reply.attribute("dependency", children);
	return reply;
};

int main()
{
  std::cout << "\n  Testing Server Prototype";
  std::cout << "\n ==========================";
  std::cout << "\n";

  Server server(serverEndPoint, "ServerPrototype");
  server.start();

  std::cout << "\n  testing getFiles and getDirs methods";
  std::cout << "\n --------------------------------------";
  Files files = server.getFiles();
  show(files, "Files:");
  Dirs dirs = server.getDirs();
  show(dirs, "Dirs:");
  std::cout << "\n";

  std::cout << "\n  testing message processing";
  std::cout << "\n ----------------------------";
  server.addMsgProc("echo", echo);
  server.addMsgProc("getFiles", getFiles);
  server.addMsgProc("getDirs", getDirs);
  server.addMsgProc("serverQuit", echo);
  server.addMsgProc("checkIn", checkIn);
  server.addMsgProc("checkOut", checkOut);
  server.addMsgProc("browse", browse);
  server.addMsgProc("metedata",meteData);
  server.addMsgProc("checkOut2", checkOut2);
  server.addMsgProc("browse2", browse2);
  server.addMsgProc("closeFile", closeFile);
  server.addMsgProc("query", query);
  server.addMsgProc("checkOutParents", checkOutParents);
  server.processMessages();
  
  Msg msg(serverEndPoint, serverEndPoint);  // send to self
  msg.name("msgToSelf");
  msg.command("echo");
  msg.attribute("verbose", "show me");
  server.postMessage(msg);
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  std::cout << "\n  press enter to exit";
  std::cin.get();
  std::cout << "\n";

  msg.command("serverQuit");
  server.postMessage(msg);
  server.stop();
  return 0;
}

