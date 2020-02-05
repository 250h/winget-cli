﻿// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
#include "pch.h"
#include "Public/AppInstallerCLICore.h"
#include "Commands/RootCommand.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace AppInstaller::CLI;

namespace AppInstaller::CLI
{

    int CoreMain(int argc, wchar_t const** argv) try
    {
        init_apartment();

        // Enable logging (*all* for now, TODO: add common arguments to allow control of logging)
        Logging::Log().EnableChannel(Logging::Channel::All);
        Logging::Log().SetLevel(Logging::Level::Verbose);
        Logging::AddFileLogger();
        Logging::EnableWilFailureTelemetry();

        Logging::Telemetry().LogStartup();

        // Convert incoming wide char args to UTF8
        std::vector<std::string> utf8Args;
        for (int i = 1; i < argc; ++i)
        {
            utf8Args.emplace_back(Utility::ConvertToUTF8(argv[i]));
        }

        AICLI_LOG(CLI, Info, << "AppInstaller CLI invoked with arguments:" << [&]() {
                std::stringstream strstr;
                for (const auto& arg : utf8Args)
                {
                    strstr << " '" << arg << '\'';
                }
                return strstr.str();
            }());

        RootCommand root;
        Invocation invocation{ std::move(utf8Args) };

        // The root command is our fallback in the event of very bad or very little input
        Command* commandToExecute = &root;
        std::unique_ptr<Command> foundCommand;

        try
        {
            foundCommand = root.FindInvokedCommand(invocation);
            if (foundCommand)
            {
                commandToExecute = foundCommand.get();
            }

            Logging::Telemetry().LogCommand(commandToExecute->Name());

            commandToExecute->ParseArguments(invocation);
            commandToExecute->ValidateArguments(invocation);
        }
        // Exceptions specific to parsing the arguments of a command
        catch (const CommandException& ce)
        {
            commandToExecute->OutputHelp(std::cout, &ce);
            AICLI_LOG(CLI, Error, << "Error encountered parsing command line: " << ce.Message());
            return APPINSTALLER_CLI_ERROR_INVALID_CL_ARGUMENTS;
        }

        try
        {
            commandToExecute->Execute(invocation, std::cout, std::cin);
        }
        // Exceptions that may occur in the process of executing an arbitrary command
        catch (const winrt::hresult_error& hre)
        {
            // TODO: Better error output
            std::string message = Utility::ConvertToUTF8(hre.message());
            std::cout << "An error occured while executing the command: " << message << std::endl;
            AICLI_LOG(CLI, Error, << "Error encountered executing command: " << message);
            return APPINSTALLER_CLI_ERROR_COMMAND_FAILED;
        }
        catch (const std::exception& e)
        {
            // TODO: Better error output
            std::cout << "An error occured while executing the command: " << e.what() << std::endl;
            AICLI_LOG(CLI, Error, << "Error encountered executing command: " << e.what());
            return APPINSTALLER_CLI_ERROR_COMMAND_FAILED;
        }

        Logging::Telemetry().LogCommandSuccess(commandToExecute->Name());
        return 0;
    }
    // End of the line exceptions that are not ever expected.
    // Telemetry cannot be reliable beyond this point, so don't let these happen.
    catch (const winrt::hresult_error&)
    {
        return APPINSTALLER_CLI_ERROR_INTERNAL_ERROR;
    }
    catch (const std::exception&)
    {
        return APPINSTALLER_CLI_ERROR_INTERNAL_ERROR;
    }
    catch (...)
    {
        return APPINSTALLER_CLI_ERROR_INTERNAL_ERROR;
    }

}