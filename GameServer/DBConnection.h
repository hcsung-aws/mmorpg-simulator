#pragma once
#include <windows.h>
#include <sqlext.h>
#include <string>
#include <functional>

class DBConnection {
public:
    DBConnection();
    ~DBConnection();

    bool Connect(const std::string& endpoint, const std::string& database,
                 const std::string& uid, const std::string& pwd);
    void Disconnect();
    bool IsConnected() const { return connected_; }

    // Execute stored procedure with failover handling
    // Returns: 0 = success, -1 = error, 1 = need retry (failover)
    int ExecuteProc(const std::wstring& procCall,
                    std::function<void(SQLHSTMT)> bindParams,
                    std::function<bool(SQLHSTMT)> processResult);

    // Simple query execution
    bool ExecuteQuery(const std::wstring& query);

    SQLHSTMT GetStmt() { return hStmt_; }

private:
    bool Reconnect();
    void HandleDiagnostic(SQLHANDLE handle, SQLSMALLINT type);
    bool CheckFailoverState(SQLHSTMT hStmt, SQLRETURN retcode);
    void SetInitialSessionState();

    SQLHENV hEnv_ = nullptr;
    SQLHDBC hDbc_ = nullptr;
    SQLHSTMT hStmt_ = nullptr;
    
    std::string endpoint_;
    std::string database_;
    std::string uid_;
    std::string pwd_;
    
    bool connected_ = false;
    static constexpr int MAX_RETRIES = 5;
};
