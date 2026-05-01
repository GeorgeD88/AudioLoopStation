//
// Created by Vincewa Tran on 4/14/26.
//

#pragma once

#include "juce_core/juce_core.h"
#include <fstream>
#include <sstream>
#include <memory>

// ENABLE/DISABLE LOGGER
#define ENABLE_DEBUG_LOGGER 0  // 1 = enabled, 0 = disabled (DISABLED to avoid crashes)

#if ENABLE_DEBUG_LOGGER

/**
 * Singleton Logger to trace all Looper events
 * Writes to a file with timestamps for debugging
 */
class DebugLogger
{
public:
    static DebugLogger& getInstance()
    {
        static DebugLogger instance;
        return instance;
    }

    // Log a simple event
    void log(const juce::String& message)
    {
        if (!isEnabled) return;

        auto timestamp = juce::Time::getCurrentTime();
        auto timestampStr = timestamp.formatted("%H:%M:%S.%f");

        juce::String fullMessage = "[" + timestampStr + "] " + message;

        // Thread-safe write
        const juce::ScopedLock lock(logLock);

        if (logFile.is_open())
        {
            logFile << fullMessage.toStdString() << std::endl;
            logFile.flush(); // Force immediate write
        }

        // Also in console for real-time debug
        DBG(fullMessage);
    }

    // Log an event with track ID
    void logTrack(int trackID, const juce::String& event, const juce::String& details = "")
    {
        juce::String msg = "TRACK " + juce::String(trackID) + ": " + event;
        if (details.isNotEmpty())
            msg += " | " + details;
        log(msg);
    }

    // Log a state change
    void logStateChange(int trackID, const juce::String& oldState, const juce::String& newState)
    {
        logTrack(trackID, "STATE CHANGE", oldState + " -> " + newState);
    }

    // Log positions and lengths
    void logPosition(int trackID, int position, int loopLength, int globalPos = -1)
    {
        juce::String details = "pos=" + juce::String(position) +
                              " len=" + juce::String(loopLength);
        if (globalPos >= 0)
            details += " global=" + juce::String(globalPos);
        logTrack(trackID, "POSITION", details);
    }

    // Log user actions (buttons)
    void logButton(const juce::String& buttonName, int trackID = -1)
    {
        juce::String msg = "BUTTON: " + buttonName;
        if (trackID >= 0)
            msg += " (Track " + juce::String(trackID) + ")";
        log(msg);
    }

    // Log important values
    void logValue(const juce::String& name, double value)
    {
        log("VALUE: " + name + " = " + juce::String(value, 3));
    }

    // Log an error or warning
    void logError(const juce::String& error)
    {
        log("*** ERROR: " + error);
    }

    void logWarning(const juce::String& warning)
    {
        log("!!! WARNING: " + warning);
    }

    // Separators for readability
    void logSeparator(const juce::String& title = "")
    {
        if (title.isEmpty())
            log("========================================");
        else
            log("======== " + title + " ========");
    }

    // Enable/disable logging
    void setEnabled(bool enabled) { isEnabled = enabled; }
    bool getEnabled() const { return isEnabled; }

    // Explicitly initialize the logger (to be called after JUCE initialization)
    void initialize()
    {
        if (isInitialized) return;

        try
        {
            // Create the log file in the Documents folder
            auto documentsDir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
            auto logFileObj = documentsDir.getChildFile("SimpleLooper_Debug.log");
            logFilePath = logFileObj.getFullPathName();

            logFile.open(logFilePath.toStdString(), std::ios::out | std::ios::app);

            if (logFile.is_open())
            {
                isInitialized = true;
                logSeparator("NEW SESSION");
                log("Log file: " + logFilePath);
                log("Start time: " + juce::Time::getCurrentTime().toString(true, true));
            }
        }
        catch (...)
        {
            isInitialized = false;
        }
    }

    // Close and reopen the file (useful to flush the buffer)
    void flush()
    {
        const juce::ScopedLock lock(logLock);
        if (logFile.is_open())
            logFile.flush();
    }

    // Clear the log file
    void clearLog()
    {
        const juce::ScopedLock lock(logLock);
        if (logFile.is_open())
        {
            logFile.close();
            logFile.open(logFilePath.toStdString(), std::ios::out | std::ios::trunc);
            log("=== LOG CLEARED ===");
        }
    }

private:
    DebugLogger()
    {
        // Do not initialize here - wait for explicit call to initialize()
        // to avoid issues with initialization order.
    }

    ~DebugLogger()
    {
        if (logFile.is_open())
        {
            try
            {
                log("=== SESSION END ===");
            }
            catch (...) {}
            logFile.close();
        }
    }

    // No copy
    DebugLogger(const DebugLogger&) = delete;
    DebugLogger& operator=(const DebugLogger&) = delete;

    std::ofstream logFile;
    juce::String logFilePath;
    juce::CriticalSection logLock;
    bool isEnabled = true;
    bool isInitialized = false;
};

// Convenience macros for logging
#define LOG(msg) DebugLogger::getInstance().log(msg)
#define LOG_TRACK(trackID, event, details) DebugLogger::getInstance().logTrack(trackID, event, details)
#define LOG_STATE(trackID, old, new) DebugLogger::getInstance().logStateChange(trackID, old, new)
#define LOG_POS(trackID, pos, len, global) DebugLogger::getInstance().logPosition(trackID, pos, len, global)
#define LOG_BUTTON(name, trackID) DebugLogger::getInstance().logButton(name, trackID)
#define LOG_VALUE(name, val) DebugLogger::getInstance().logValue(name, val)
#define LOG_ERROR(msg) DebugLogger::getInstance().logError(msg)
#define LOG_WARNING(msg) DebugLogger::getInstance().logWarning(msg)
#define LOG_SEP(title) DebugLogger::getInstance().logSeparator(title)

#else

// Logger disabled - empty macros that do nothing
#define LOG(msg) ((void)0)
#define LOG_TRACK(trackID, event, details) ((void)0)
#define LOG_STATE(trackID, old, new) ((void)0)
#define LOG_POS(trackID, pos, len, global) ((void)0)
#define LOG_BUTTON(name, trackID) ((void)0)
#define LOG_VALUE(name, val) ((void)0)
#define LOG_ERROR(msg) ((void)0)
#define LOG_WARNING(msg) ((void)0)
#define LOG_SEP(title) ((void)0)

// Empty class to avoid compilation errors
class DebugLogger
{
public:
    static DebugLogger& getInstance() { static DebugLogger instance; return instance; }
    void initialize() {}
    void log(const juce::String&) {}
    void setEnabled(bool) {}
};

#endif