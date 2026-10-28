
/*
 * 
 *
 * Template file.
 * You will need to add more code here to execute the command table.
 *
 * NOTE: You are responsible for fixing any bugs this code may have!
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <glob.h>
#include <wordexp.h>
#include <vector>
#include <string>
#include <cctype>
#include <iostream>
#include <sstream>
#include "command.h"
#include "tokenizer.h"
#include <errno.h>


static int childLogFd = -1;

// Append text to a buffer without using printf.
static size_t appendText(char *buffer, size_t pos,
                         const char *text)
{
    while (*text != '\0')
    {
        buffer[pos++] = *text++;
    }

    return pos;
}

// Convert a positive process ID into decimal digits.
static size_t appendPid(char *buffer, size_t pos, pid_t pid)
{
    char digits[32];
    int count = 0;
    unsigned long value = (unsigned long)pid;

    do
    {
        digits[count++] = '0' + (value % 10);
        value /= 10;
    }
    while (value > 0);

    while (count > 0)
    {
        buffer[pos++] = digits[--count];
    }

    return pos;
}


static void logChildTermination(pid_t pid)
{
    if (childLogFd == -1)
        return;

    char buffer[128];
    size_t length = 0;

    length = appendText(buffer, length, "Child PID ");
    length = appendPid(buffer, length, pid);
    length = appendText(buffer, length, " terminated\n");

    write(childLogFd, buffer, length);
}

void handleChildSignal(int signal)
{
    (void)signal;

    int savedErrno = errno;
    int status;
    pid_t pid;

  
while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
{
    logChildTermination(pid);
}


    errno = savedErrno;
}




void parse(std::vector<Token> &tokens)
{
    Command &cmd = Command::_currentCommand;

    // Start with a clean command.
    cmd.clear();

    SimpleCommand *simple = new SimpleCommand();
    bool hasError = false;

    for (int i = 0; i < (int)tokens.size(); i++)
    {
        Token token = tokens[i];

        if (token.type == TOKEN_EOF)
        {
            break;
        }

         
   
if (token.type == TOKEN_COMMAND ||
    token.type == TOKEN_ARGUMENT)
{
    std::string argumentValue = token.value;
    bool isQuoted = false;

    // Join tokens if a quoted argument contains spaces.
    if (!argumentValue.empty() &&
        (argumentValue[0] == '"' || argumentValue[0] == '\''))
    {
        isQuoted = true;
        char quote = argumentValue[0];

        while (argumentValue.size() < 2 ||
               argumentValue[argumentValue.size() - 1] != quote)
        {
            if (i + 1 >= (int)tokens.size() ||
                tokens[i + 1].type == TOKEN_EOF)
            {
                fprintf(stderr, "Syntax error: unmatched quote\n");
                hasError = true;
                break;
            }

            argumentValue += " ";
            argumentValue += tokens[++i].value;
        }

        if (hasError)
            break;

        // Remove the opening and closing quotation marks.
        argumentValue = argumentValue.substr(
            1, argumentValue.size() - 2);
    }

    // Expand wildcards only for unquoted arguments.
    if (!isQuoted &&
        (argumentValue.find('*') != std::string::npos ||
         argumentValue.find('?') != std::string::npos))
    {
        glob_t matches;
        int result = glob(argumentValue.c_str(), 0, NULL, &matches);

        if (result == 0 && matches.gl_pathc > 0)
        {
            for (size_t j = 0; j < matches.gl_pathc; j++)
            {
                simple->insertArgument(
                    strdup(matches.gl_pathv[j]));
            }

            globfree(&matches);
        }
        else
        {
            globfree(&matches);
            simple->insertArgument(strdup(argumentValue.c_str()));
        }
    }
    else
    {
        simple->insertArgument(strdup(argumentValue.c_str()));
    }
}



        // Pipe: finish the current simple command
        else if (token.type == TOKEN_PIPE)
        {
            if (simple->_numberOfArguments == 0)
            {
                fprintf(stderr, "Syntax error: missing command before pipe\n");
                hasError = true;
                break;
            }

            cmd.insertSimpleCommand(simple);
            simple = new SimpleCommand();
        }

        // Background execution
        else if (token.type == TOKEN_BACKGROUND)
        {
            cmd._background = 1;

            // Background marker must be at the end.
            if (i != (int)tokens.size() - 1 &&
                tokens[i + 1].type != TOKEN_EOF)
            {
                fprintf(stderr, "Syntax error: '&' must be at the end\n");
                hasError = true;
                break;
            }
        }

        // Input, output, append, and error redirection
        else if (token.type == TOKEN_REDIRECT ||
                 token.type == TOKEN_APPEND ||
                 token.type == TOKEN_INPUT ||
                 token.type == TOKEN_ERROR ||
                 token.type == TOKEN_REDIRECT_AND_ERROR)
        {
            if (i + 1 >= (int)tokens.size() ||
                tokens[i + 1].type == TOKEN_EOF ||
                tokens[i + 1].type == TOKEN_PIPE ||
                tokens[i + 1].type == TOKEN_BACKGROUND ||
                tokens[i + 1].type == TOKEN_REDIRECT ||
                tokens[i + 1].type == TOKEN_APPEND ||
                tokens[i + 1].type == TOKEN_INPUT ||
                tokens[i + 1].type == TOKEN_ERROR ||
                tokens[i + 1].type == TOKEN_REDIRECT_AND_ERROR)
            {
                fprintf(stderr, "Syntax error: missing redirection filename\n");
                hasError = true;
                break;
            }

            char *filename = strdup(tokens[i + 1].value.c_str());

            if (token.type == TOKEN_INPUT)
            {
                if (cmd._inputFile != NULL)
                {
                    fprintf(stderr, "Syntax error: duplicate input redirection\n");
                    free(filename);
                    hasError = true;
                    break;
                }

                cmd._inputFile = filename;
            }
            else if (token.type == TOKEN_REDIRECT)
            {
                if (cmd._outFile != NULL)
                {
                    fprintf(stderr, "Syntax error: duplicate output redirection\n");
                    free(filename);
                    hasError = true;
                    break;
                }

                cmd._outFile = filename;
                cmd._append = 0;
            }
            else if (token.type == TOKEN_APPEND)
            {
                if (cmd._outFile != NULL)
                {
                    fprintf(stderr, "Syntax error: duplicate output redirection\n");
                    free(filename);
                    hasError = true;
                    break;
                }

                cmd._outFile = filename;
                cmd._append = 1;
            }
            else if (token.type == TOKEN_ERROR)
            {
                if (cmd._errFile != NULL)
                {
                    fprintf(stderr, "Syntax error: duplicate error redirection\n");
                    free(filename);
                    hasError = true;
                    break;
                }

                cmd._errFile = filename;
            }
            else if (token.type == TOKEN_REDIRECT_AND_ERROR)
            {
                if (cmd._outFile != NULL || cmd._errFile != NULL)
                {
                    fprintf(stderr, "Syntax error: duplicate output/error redirection\n");
                    free(filename);
                    hasError = true;
                    break;
                }

                cmd._outFile = strdup(tokens[i + 1].value.c_str());
                cmd._errFile = filename;
                cmd._out_error = 1;
                cmd._append = 1;
            }

            // The next token was consumed as the filename.
            i++;
        }
        else
        {
            fprintf(stderr, "Syntax error: unexpected token '%s'\n",
                    token.value.c_str());
            hasError = true;
            break;
        }
    }

    if (!hasError)
    {
        if (simple->_numberOfArguments > 0)
        {
            cmd.insertSimpleCommand(simple);
            simple = NULL;
        }
        else if (cmd._numberOfSimpleCommands > 0)
        {
            fprintf(stderr, "Syntax error: missing command after pipe\n");
            hasError = true;
        }
    }

    // Free a simple command that was not inserted into cmd.
    if (simple != NULL)
    {
        for (int j = 0; j < simple->_numberOfArguments; j++)
        {
            free(simple->_arguments[j]);
        }

        free(simple->_arguments);
        delete simple;
    }

    if (hasError)
    {
        cmd.clear();
        return;
    }

    // Task 1: display the parsed command table.
    if (cmd._numberOfSimpleCommands > 0)
{
    cmd.execute();
}
}


SimpleCommand::SimpleCommand()
{
    _numberOfAvailableArguments = 5;
    _numberOfArguments = 0;
    _arguments = (char **)malloc(_numberOfAvailableArguments * sizeof(char *));
}

void SimpleCommand::insertArgument(char *argument)
{
    if (_numberOfAvailableArguments == _numberOfArguments + 1)
    {
        _numberOfAvailableArguments *= 2;
        _arguments = (char **)realloc(_arguments,
                                      _numberOfAvailableArguments * sizeof(char *));
    }

    _arguments[_numberOfArguments] = argument;
    _arguments[_numberOfArguments + 1] = NULL;

    _numberOfArguments++;
}


Command::Command()
{
    _numberOfAvailableSimpleCommands = 1;
    _numberOfSimpleCommands = 0;

    _simpleCommands = (SimpleCommand **)
        malloc(_numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));

    _outFile = NULL;
    _inputFile = NULL;
    _errFile = NULL;

    _out_error = 0;
    _background = 0;
    _append = 0;
}


void Command::insertSimpleCommand(SimpleCommand *simpleCommand)
{
    if (_numberOfAvailableSimpleCommands == _numberOfSimpleCommands)
    {
        _numberOfAvailableSimpleCommands *= 2;
        _simpleCommands = (SimpleCommand **)realloc(_simpleCommands,
                                                    _numberOfAvailableSimpleCommands * sizeof(SimpleCommand *));
    }

    _simpleCommands[_numberOfSimpleCommands] = simpleCommand;
    _numberOfSimpleCommands++;
}

void Command::clear()
{
    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            free(_simpleCommands[i]->_arguments[j]);
        }

        free(_simpleCommands[i]->_arguments);
        free(_simpleCommands[i]);
    }

    if (_outFile)
    {
        free(_outFile);
    }

    if (_inputFile)
    {
        free(_inputFile);
    }

    if (_errFile)
    {
        free(_errFile);
    }

    _numberOfSimpleCommands = 0;
    _outFile = NULL;
    _inputFile = NULL;
    _errFile = NULL;
    _background = 0;
    _out_error = 0;
    _append = 0;
}

void Command::print()
{
    printf("\n\n");
    printf("              COMMAND TABLE                \n");
    printf("\n");
    printf("  #   Simple Commands\n");
    printf("  --- ----------------------------------------------------------\n");

    for (int i = 0; i < _numberOfSimpleCommands; i++)
    {
        printf("  %-3d ", i);
        for (int j = 0; j < _simpleCommands[i]->_numberOfArguments; j++)
        {
            printf("\"%s\" \t", _simpleCommands[i]->_arguments[j]);
        }
    }

    printf("\n\n");
    printf("  Output       Input        Error        Err&Out       Background\n");
    printf("  ------------ ------------ ------------ ------------ ------------\n");
    printf("  %-12s %-12s %-12s %-12s %-12s\n", _outFile ? _outFile : "default",
           _inputFile ? _inputFile : "default", _errFile ? _errFile : "default", _out_error == 1 ? _errFile : "default"
           ,_background ? "YES" : "NO");
    printf("\n\n");
}




void Command::execute()
{
    if (_numberOfSimpleCommands == 0)
    {
        clear();
        return;
    }

    // Built-in: exit
   if (_numberOfSimpleCommands == 1 &&
    strcmp(_simpleCommands[0]->_arguments[0], "exit") == 0)
{
    printf("Good bye!!\n");
    fflush(stdout);
    clear();
    exit(0);
}

    // Built-in: cd
    if (_numberOfSimpleCommands == 1 &&
        strcmp(_simpleCommands[0]->_arguments[0], "cd") == 0)
    {
        char *directory = _simpleCommands[0]->_arguments[1];

        if (directory == NULL)
            directory = getenv("HOME");

        if (directory == NULL || chdir(directory) != 0)
            perror("cd");

        clear();
        return;
    }

    int numberOfCommands = _numberOfSimpleCommands;
    int previousRead = -1;
    pid_t *pids = (pid_t *)malloc(numberOfCommands * sizeof(pid_t));

    if (pids == NULL)
    {
        perror("malloc");
        clear();
        return;
    }

    for (int i = 0; i < numberOfCommands; i++)
    {
        int pipefd[2] = {-1, -1};

        // Create a pipe unless this is the last command.
        if (i < numberOfCommands - 1)
        {
            if (pipe(pipefd) < 0)
            {
                perror("pipe");
                if (previousRead != -1)
                    close(previousRead);
                free(pids);
                clear();
                return;
            }
        }

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            if (previousRead != -1)
                close(previousRead);
            if (pipefd[0] != -1)
                close(pipefd[0]);
            if (pipefd[1] != -1)
                close(pipefd[1]);

            for (int j = 0; j < i; j++)
                waitpid(pids[j], NULL, 0);

            free(pids);
            clear();
            return;
        }

        if (pid == 0)
        {
            // Connect previous command's pipe to standard input.
            if (previousRead != -1)
            {
                if (dup2(previousRead, STDIN_FILENO) < 0)
                {
                    perror("dup2 pipe input");
                    _exit(1);
                }
            }

            // Connect standard output to the next command's pipe.
            if (i < numberOfCommands - 1)
            {
                if (dup2(pipefd[1], STDOUT_FILENO) < 0)
                {
                    perror("dup2 pipe output");
                    _exit(1);
                }
            }

            if (previousRead != -1)
                close(previousRead);
            if (pipefd[0] != -1)
                close(pipefd[0]);
            if (pipefd[1] != -1)
                close(pipefd[1]);
            
            signal(SIGINT, SIG_DFL);

            // Input redirection.
            // Explicit redirection overrides the pipeline input.
            if (i == 0 && _inputFile != NULL)
            {
                int fd = open(_inputFile, O_RDONLY);

                if (fd < 0)
                {
                    perror("open input file");
                    _exit(1);
                }

                if (dup2(fd, STDIN_FILENO) < 0)
                {
                    perror("dup2 input");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            // Output redirection.
            // Explicit redirection overrides the pipeline output.
            if (i == numberOfCommands - 1 && _outFile != NULL)
            {
                int flags = O_WRONLY | O_CREAT;

                if (_append)
                    flags |= O_APPEND;
                else
                    flags |= O_TRUNC;

                int fd = open(_outFile, flags, 0644);

                if (fd < 0)
                {
                    perror("open output file");
                    _exit(1);
                }

                if (dup2(fd, STDOUT_FILENO) < 0)
                {
                    perror("dup2 output");
                    close(fd);
                    _exit(1);
                }

                if (_out_error && dup2(fd, STDERR_FILENO) < 0)
                {
                    perror("dup2 error output");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            // Separate error redirection.
            if (i == numberOfCommands - 1 &&
                _errFile != NULL && !_out_error)
            {
                int fd = open(_errFile,
                              O_WRONLY | O_CREAT | O_TRUNC, 0644);

                if (fd < 0)
                {
                    perror("open error file");
                    _exit(1);
                }

                if (dup2(fd, STDERR_FILENO) < 0)
                {
                    perror("dup2 error");
                    close(fd);
                    _exit(1);
                }

                close(fd);
            }

            execvp(_simpleCommands[i]->_arguments[0],
                   _simpleCommands[i]->_arguments);

            perror("execvp");
            _exit(127);
        }

        // Parent process.
        pids[i] = pid;

        if (previousRead != -1)
            close(previousRead);

        if (pipefd[1] != -1)
            close(pipefd[1]);

        previousRead = pipefd[0];
    }

    if (previousRead != -1)
        close(previousRead);

    
if (_background)
{
    // Background pipeline: do not wait here.
    printf("Started background job\n");
}
else
{
    // Foreground pipeline: wait for every process.
   
for (int i = 0; i < numberOfCommands; i++)
{
    while (true)
    {
        pid_t result = waitpid(pids[i], NULL, 0);

        if (result == pids[i])
{
    logChildTermination(result);
    break;
}

        if (result == -1 && errno == EINTR)
        {
            continue;
        }

        if (result == -1 && errno == ECHILD)
        {
            // The SIGCHLD handler already reaped this child.
            break;
        }

        if (result == -1)
        {
            perror("waitpid");
        }

        break;
    }
}

}

free(pids);
clear();

}




void Command::prompt()
{
    std::string input;

    while (true)
    {
        printf("myshell>");
        fflush(stdout);

        if (!std::getline(std::cin, input))
        {
            break;
        }

        std::vector<Token> tokens = tokenize(input);
        parse(tokens);
    }
}

Command Command::_currentCommand;
SimpleCommand *Command::_currentSimpleCommand;



int main()
{
    // Create or open the child termination log.
    childLogFd = open(
        "child_termination.log",
        O_WRONLY | O_CREAT | O_APPEND,
        0644
    );

    if (childLogFd == -1)
    {
        perror("open child termination log");
        return 1;
    }

    // Do not keep the log file open across exec().
    fcntl(childLogFd, F_SETFD, FD_CLOEXEC);

    // The shell ignores Ctrl+C.
    signal(SIGINT, SIG_IGN);

    // Collect terminated child processes.
    signal(SIGCHLD, handleChildSignal);

    Command::_currentCommand.prompt();

    close(childLogFd);
    return 0;
}


