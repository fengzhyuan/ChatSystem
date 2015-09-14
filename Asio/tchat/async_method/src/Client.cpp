/*!
 * \file chatClient.cpp
 * 
 */
#include "tchat/async_method/include/Client.h"

Client::Client(io_service& service,
                tcp::resolver::iterator iter)
    : m_service(service),
      m_socket(service),
      m_status(true) {
    cout << "input name:" << endl;
    cin >> m_uid; // TODO: check validity on server

    boost::asio::async_connect(m_socket, iter,
        boost::bind(&Client::hConnect, this,
          boost::asio::placeholders::error));
  }

/**
 * \TODO check if timeout during ctor initialization
 * // boost::asio::deadline_timer
 * @return return false if ctor failed
 */
bool Client::isConnected() const {
    return m_status;
}

/**
 * 
 * @param _msg message to be stored
 */
void Client::write(const Message& msg) {
    m_service.post(boost::bind(&Client::doWrite, this, msg));
}

void Client::close() {
    m_service.post(boost::bind(&Client::doClose, this));
}

/**
 * cstr connection handler
 * @param err
 */
void Client::hConnect(const system::error_code& err) {
    if (!err) {
        cout << "connected to server " << endl;
        fflush(stdout);
         // push basic info to server
        Message msg(Message::MSG_INIT);
        msg.buildMsg(this->m_uid);
        //mClientInfo.encodeHeader();
        m_msgs.push_back(msg);
        
        hRead(err);
    }   
}

void Client::hRead(const system::error_code& error) {
    if (!error) {
        // exec pParseHeader after read operation
        async_read(m_socket,
            buffer(m_msg_in.data(), Message::HEADER_LENGTH),
            bind(&Client::hParseHeader, this,
                asio::placeholders::error));
    }
}
/**
 * sub-module of HandleParseBody
 * @param error error code (implicitly convert to bool type)
 */
void Client::hParseHeader(const boost::system::error_code& error) {
    if (!error && m_msg_in.decodeHeader()) {
      boost::asio::async_read(m_socket,
          boost::asio::buffer(m_msg_in.body(), m_msg_in.bodyLength()),
          boost::bind(&Client::hParseBody, this,
            boost::asio::placeholders::error));
    }
    else {
        doClose();
    }
}

/**
 * call parse_header hr first
 * @param err
 */
void Client::hParseBody(const boost::system::error_code& error) {
    if (!error) {
        displayMsg();
        boost::asio::async_read(m_socket,
            boost::asio::buffer(m_msg_in.data(), Message::HEADER_LENGTH),
            boost::bind(&Client::hParseHeader, this,
              boost::asio::placeholders::error));
    }
    else {
        doClose();
    }
}

/**
 * write all messages into socket
 * @param err
 */
void Client::doWrite(Message msg) {
    bool bEmpty = m_msgs.empty();
    m_msgs.push_back(msg);
    
//    if (bEmpty) {
        boost::asio::async_write(m_socket,
            boost::asio::buffer(m_msgs.front().data(),
              m_msgs.front().length()),
            boost::bind(&Client::hWrite, this,
              boost::asio::placeholders::error));
//    }
}

void Client::hWrite(const boost::system::error_code& error) {
    if  (!error) {
        m_msgs.pop_front();
        if  (!m_msgs.empty()) {
            boost::asio::async_write(m_socket,
                boost::asio::buffer(m_msgs.front().data(),
                  m_msgs.front().length()),
                boost::bind(&Client::hWrite, this,
                  boost::asio::placeholders::error));
        }
    }
    else {
        doClose();
    }
}

void Client::doClose() {
    m_socket.close();
}

const string Client::getName() const {
    return m_uid;
}
void Client::displayMsg() {
    using namespace Color;
    Color::Modifier cfont;
    int msgType = m_msg_in.msgType();
    switch (msgType) {
        case Message::MSG_ENTER :
            cfont = Color::Modifier(Color::FG_GREEN); break;
        case Message::MSG_LEAVE :
            cfont = Color::Modifier(Color::FG_RED); break;
        default:
            cfont = Color::Modifier(Color::FG_DEFAULT);
    }
    Color::Modifier cref, cline = Color::Modifier(Color::FG_BLUE);
    
    char body[Message::MAX_BODY_LENGTH+1]="";
    memcpy(body, m_msg_in.body(), m_msg_in.bodyLength());
    if (msgType == Message::MSG_CONT)
        cout << cline << "----------------------------" << endl;
    cout << cfont << body << cref << endl;
    if (msgType == Message::MSG_CONT)
        cout << cline << "----------------------------" << cref << endl;
}

/**
 * client context initialization
 * @param argc main argc
 * @param argv main argv 
 * @return [true: cstr established][false: cstr failed]
 */
bool initClientContext(int argc, char**argv) {
    try{
        if (argc != 4) {
            cout << " invalid param [<host> <port>]\n";
            return false;
        }
        
        io_service mService;

        tcp::resolver resolver(mService);
        tcp::resolver::query mQuery(argv[2], argv[3]);
        // domain name: 1-to-many
        tcp::resolver::iterator mIterator = resolver.resolve(mQuery);

        boost::shared_ptr<Client> pClient(new Client(mService, mIterator));

        boost::thread mThread(boost::bind(&io_service::run, &mService));

        char input[Message::MAX_BODY_LENGTH + 1];
        while (cin.getline(input, Message::MAX_BODY_LENGTH + 1)) {
            if (!strcmp(input, "EXIT") ) {
                break;
            } 
            else if (!strlen(input)) {
                continue;
            }
            
            Message msg;
            string strMsg = pClient->getName() + ":" + string(input);

            msg.buildMsg(strMsg);
            
            pClient->write(msg);
        }

        pClient->close();
        mThread.join();
    }
    catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return false;
    }
    return true;
}
