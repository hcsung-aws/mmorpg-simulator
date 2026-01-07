#include "DBConnection.h"
#include <iostream>

DBConnection::DBConnection() {}

DBConnection::~DBConnection() {
    Disconnect();
}

bool DBConnection::Connect(const std::string& endpoint, const std::string& database,
                           const std::string& uid, const std::string& pwd) {
    endpoint_ = endpoint;
    database_ = database;
    uid_ = uid;
    pwd_ = pwd;

    // Allocate environment handle
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &hEnv_) != SQL_SUCCESS) {
        std::cerr << "Failed to allocate environment handle" << std::endl;
        return false;
    }

    SQLSetEnvAttr(hEnv_, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    // Allocate connection handle
    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv_, &hDbc_) != SQL_SUCCESS) {
        HandleDiagnostic(hEnv_, SQL_HANDLE_ENV);
        return false;
    }

    // Build connection string with AWS ODBC Driver and failover mode
    char connStr[512];
    snprintf(connStr, sizeof(connStr),
        "DRIVER={AWS ODBC ANSI Driver for MySQL};"
        "SERVER=%s;DATABASE=%s;UID=%s;PWD=%s;"
        "FAILOVER_MODE={strict reader}",
        endpoint.c_str(), database.c_str(), uid.c_str(), pwd.c_str());

    SQLRETURN ret = SQLDriverConnectA(hDbc_, NULL, (SQLCHAR*)connStr, SQL_NTS,
                                       NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        HandleDiagnostic(hDbc_, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
        return false;
    }

    // Allocate statement handle
    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt_) != SQL_SUCCESS) {
        HandleDiagnostic(hDbc_, SQL_HANDLE_DBC);
        SQLDisconnect(hDbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
        return false;
    }

    // Set query timeout
    SQLSetStmtAttr(hStmt_, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);

    connected_ = true;
    std::cout << "DB Connected: " << endpoint << "/" << database << std::endl;
    return true;
}

void DBConnection::Disconnect() {
    if (hStmt_) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt_);
        hStmt_ = nullptr;
    }
    if (hDbc_) {
        SQLDisconnect(hDbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
    }
    if (hEnv_) {
        SQLFreeHandle(SQL_HANDLE_ENV, hEnv_);
        hEnv_ = nullptr;
    }
    connected_ = false;
}

bool DBConnection::Reconnect() {
    std::cout << "Attempting to reconnect..." << std::endl;
    
    if (hStmt_) {
        SQLFreeHandle(SQL_HANDLE_STMT, hStmt_);
        hStmt_ = nullptr;
    }
    if (hDbc_) {
        SQLDisconnect(hDbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
    }

    // Reallocate connection
    if (SQLAllocHandle(SQL_HANDLE_DBC, hEnv_, &hDbc_) != SQL_SUCCESS) {
        return false;
    }

    char connStr[512];
    snprintf(connStr, sizeof(connStr),
        "DRIVER={AWS ODBC ANSI Driver for MySQL};"
        "SERVER=%s;DATABASE=%s;UID=%s;PWD=%s;"
        "FAILOVER_MODE={strict reader}",
        endpoint_.c_str(), database_.c_str(), uid_.c_str(), pwd_.c_str());

    SQLRETURN ret = SQLDriverConnectA(hDbc_, NULL, (SQLCHAR*)connStr, SQL_NTS,
                                       NULL, 0, NULL, SQL_DRIVER_NOPROMPT);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        HandleDiagnostic(hDbc_, SQL_HANDLE_DBC);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
        return false;
    }

    if (SQLAllocHandle(SQL_HANDLE_STMT, hDbc_, &hStmt_) != SQL_SUCCESS) {
        SQLDisconnect(hDbc_);
        SQLFreeHandle(SQL_HANDLE_DBC, hDbc_);
        hDbc_ = nullptr;
        return false;
    }

    SQLSetStmtAttr(hStmt_, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);
    connected_ = true;
    std::cout << "Reconnected successfully" << std::endl;
    return true;
}

void DBConnection::HandleDiagnostic(SQLHANDLE handle, SQLSMALLINT type) {
    SQLWCHAR sqlState[6], message[1000];
    SQLINTEGER nativeError;
    SQLSMALLINT i = 1;

    while (SQLGetDiagRecW(type, handle, i++, sqlState, &nativeError, 
                          message, sizeof(message)/sizeof(SQLWCHAR), NULL) == SQL_SUCCESS) {
        std::wcerr << L"SQLState: " << sqlState 
                   << L", Error: " << nativeError 
                   << L", Msg: " << message << std::endl;
    }
}

bool DBConnection::CheckFailoverState(SQLHSTMT hStmt, SQLRETURN retcode) {
    if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
        return false; // No failover needed
    }

    SQLWCHAR sqlState[6];
    SQLINTEGER nativeError;
    if (SQLGetDiagRecW(SQL_HANDLE_STMT, hStmt, 1, sqlState, &nativeError, 
                       NULL, 0, NULL) != SQL_SUCCESS) {
        return false;
    }

    // 08S02: Failover occurred - reconfigure session and retry
    if (wcsncmp(sqlState, L"08S02", 5) == 0) {
        std::cout << "Failover detected (08S02), reconfiguring session..." << std::endl;
        SetInitialSessionState();
        return true;
    }

    // 08S01: Connection lost - need full reconnect
    if (wcsncmp(sqlState, L"08S01", 5) == 0) {
        std::cout << "Connection lost (08S01), reconnecting..." << std::endl;
        connected_ = false;
        for (int i = 0; i < MAX_RETRIES; i++) {
            if (Reconnect()) {
                return true;
            }
            Sleep(100);
        }
    }

    // HY000 with 1836: Connected to read replica
    if (wcsncmp(sqlState, L"HY000", 5) == 0 && nativeError == 1836) {
        std::cout << "Connected to read-only instance, retrying..." << std::endl;
        Sleep(100);
        return true;
    }

    return false;
}

void DBConnection::SetInitialSessionState() {
    if (hStmt_) {
        SQLExecDirectA(hStmt_, (SQLCHAR*)"SET time_zone = '+09:00'", SQL_NTS);
    }
}

int DBConnection::ExecuteProc(const std::wstring& procCall,
                              std::function<void(SQLHSTMT)> bindParams,
                              std::function<bool(SQLHSTMT)> processResult) {
    if (!connected_ || !hStmt_) {
        return -1;
    }

    int retries = 0;
    while (retries < MAX_RETRIES) {
        // Bind parameters if provided
        if (bindParams) {
            bindParams(hStmt_);
        }

        SQLRETURN ret = SQLExecDirectW(hStmt_, (SQLWCHAR*)procCall.c_str(), SQL_NTS);
        
        if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
            bool success = true;
            if (processResult) {
                success = processResult(hStmt_);
            }
            SQLCloseCursor(hStmt_);
            SQLFreeStmt(hStmt_, SQL_RESET_PARAMS);
            return success ? 0 : -1;
        }

        HandleDiagnostic(hStmt_, SQL_HANDLE_STMT);

        if (CheckFailoverState(hStmt_, ret)) {
            retries++;
            continue;
        }

        SQLCloseCursor(hStmt_);
        SQLFreeStmt(hStmt_, SQL_RESET_PARAMS);
        return -1;
    }

    return -1;
}

bool DBConnection::ExecuteQuery(const std::wstring& query) {
    return ExecuteProc(query, nullptr, nullptr) == 0;
}
