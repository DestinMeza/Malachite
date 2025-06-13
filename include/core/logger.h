#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <chrono>
#include <iostream>
#include <stack>

#define MAL_USE_LOGGER 1
#if MAL_USE_LOGGER

#define MAL_LOG_TRACE(...) malachite::logger::getCurrentLogger().logMessage(__VA_ARGS__, NULL)
#define MAL_LOG_ERROR(...) malachite::logger::getCurrentLogger().logErrorMessage(__VA_ARGS__, NULL)

#define MAL_ASSERT(condition, ...) \
    if (!(condition)) MAL_LOG_ERROR(__VA_ARGS__, NULL);      
#else
#define MAL_LOG_TRACE(...)
#define MAL_LOG_ERROR(...)

#define MAL_ASSERT(condition, ...)
#endif

namespace malachite
{
    struct messageLog
    {
        std::chrono::system_clock::time_point messageLoggedTime;
        bool isOpen = true;

        char *getTime()
        {
            const std::time_t t_c = std::chrono::system_clock::to_time_t(messageLoggedTime);
            return std::ctime(&t_c);
        }
    };

    //Simple Logger class to just debug and log state to console. 
    //TODO: add log file writing.
    class logger
    {
        static messageLog createMessage()
        {
            return s_currentLogger.addLog();
        }

    public:
        static void setCurrentLogger(std::string loggerName)
        {
            bool isLoggerFound = false;
            for (const auto &viewlogger : s_loggers)
            {
                if (viewlogger.m_loggerName == loggerName)
                {
                    s_currentLogger = viewlogger;
                    isLoggerFound = true;
                    break;
                }
            }

            if (isLoggerFound)
            {
                return;
            }

            s_loggers.push_back(logger(loggerName));
        }

        static logger &getCurrentLogger()
        {
            return s_currentLogger;
        }

        static int getCurrentLoggerIndex(std::string loggerName)
        {
            int index = -1;

            int iter;
            for (const auto &viewlogger : s_loggers)
            {
                if (viewlogger.m_loggerName == loggerName)
                {
                    index = iter;
                    break;
                }

                iter++;
            }

            return index;
        }

    private:
        static logger s_currentLogger;
        static std::vector<logger> s_loggers;

    public:
        logger(std::string loggerName)
            : m_loggerName(loggerName)
        {
        }

        messageLog addLog()
        {
            messageLog loggedMessage = messageLog{std::chrono::system_clock::now()};
            m_messageLogs.push(loggedMessage);
            return loggedMessage;
        }

        void firstAddOutputInfo()
        {
            messageLog currentLoggedMessage = createMessage();
            std::string time = currentLoggedMessage.getTime();

            std::string outputMessage;
            std::string userName = "| " + s_currentLogger.m_loggerName + ": " + time + " > ";
            outputMessage.append(userName);
            std::cout << outputMessage;
            hasAddedOutputInfo = true;
        }

        template <typename T>
        void logMessage(const T& arg)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            if(NULL == arg)
            {
                std::cout << std::endl;
                hasAddedOutputInfo = false;
                return;
            }

            std::cout << arg;
        }

        void logMessage(const std::filesystem::path& path)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            std::cout << path;
        }

        void logMessage(const std::string& str)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            std::cout << str;
        }

        template <typename T, typename... Args>
        void logMessage(const T& arg, const Args& ...args)
        {
            logMessage(arg);
            logMessage(args...);
        }

        void logErrorMessage(const std::string& str)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            std::cerr << str;
        }

        void logErrorMessage(const std::filesystem::path& path)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            std::cerr << path;
        }

        template <typename T>
        void logErrorMessage(const T& arg)
        {
            if (!hasAddedOutputInfo)
            {
                firstAddOutputInfo();
            }

            if(NULL == arg)
            {
                std::cout << std::endl;
                hasAddedOutputInfo = false;
                return;
            }

            std::cerr << arg;
        }

        template <typename T, typename... Args>
        void logErrorMessage(const T& arg, const Args& ...args)
        {
            logErrorMessage(arg);
            logErrorMessage(args...);
        }

    private:
        bool hasAddedOutputInfo = false;
        std::string m_loggerName;
        std::stack<messageLog> m_messageLogs;
    };
}