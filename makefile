server: main.cpp  ./timer/lst_timer.cpp ./http/http_conn.cpp ./mysqlpool/sql_connection_pool.cpp  webserver.cpp config.h
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server
