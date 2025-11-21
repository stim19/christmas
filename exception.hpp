#include <stdexcept>
#include <string>
#include <common.hpp>

namespace Engine {

    class DatabaseException : public std::runtime_error {
        private:
            int error_code;
        public:
            DatabaseException(const std::string& msg, int code) : 
                std::runtime_error(msg), error_code(code) {}
            int get_code() const
            { return error_code; }
            ~DatabaseException() noexcept override = default;
    };
    //Connection error
    class ConnectionError : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Transaction error
    class TransactionError : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Thrown when an error occurs while preparing/parsing SQL
    class SyntaxError : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Thrown when an error occurs during binding like SQLITE_RANGE
    class BindRangeException : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Thrown when a query attempts an operation that's not allowed
    class ConstraintError : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Thrown when engine runs out of necessary recources (e.g., memory, diskspace)
    class ResourceException : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
    //Thrown when any attemps to call operational methods after a statement is finalized
    class StatementStateError : public DatabaseException {
        using DatabaseException::DatabaseException;
    };
}
