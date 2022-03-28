#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define BUFLEN 1024   /* コマンド用のバッファの大きさ */
#define MAXARGNUM 256 /* 最大の引数の数 */

typedef struct dir_list
{
    char dir_name[256];
    struct dir_list *next;
} DIR_LIST;

typedef struct history_list
{
    char command_name[256];
    int order;
    struct history_list *next;
} HISTORY_LIST;

typedef struct alias_list
{
    char original[128];
    char sub[128];
    struct alias_list *next;
} ALIAS_LIST;

int parse(char[], char *[]);
void execute_command(char *[], int, DIR_LIST **root, HISTORY_LIST **top);
void cd_command(char *args[]);
void pushd_command(DIR_LIST **root);
void dirs_command(DIR_LIST **root);
void popd_command(DIR_LIST **root);
void memory_command(HISTORY_LIST **top, char command_buffer[], int n);
void history_command(HISTORY_LIST **top);
void pre_command(char command_buffer[], HISTORY_LIST **top);
void string_command(char command_buffer[], HISTORY_LIST **top);
void wildcard_command(char *args[]);
void prompt_command(char prompt[], char *args[]);
void alias_command(char *args[], ALIAS_LIST **head);
void unalias_command(char *args[], ALIAS_LIST **head);
void aliascheck(char command_buffer[], ALIAS_LIST **head);
void cleardirlist(DIR_LIST **root);
void clearhislist(HISTORY_LIST **top);
void clearaliaslist(ALIAS_LIST **head);
void grep_command(char *args[]);
void cat_command(char *args[]);
void norder_command(char command_buffer[], HISTORY_LIST **top);
void npre_command(char command_buffer[], HISTORY_LIST **top);

/*----------------------------------------------------------------------------
 *
 *  関数名   : main
 *
 *  作業内容 : シェルのプロンプトを実現する
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    char command_buffer[BUFLEN]; /* コマンド用のバッファ */
    char *args[MAXARGNUM];       /* 引数へのポインタの配列 */
    int command_status;          /* コマンドの状態を表す
                                    command_status = 0 : フォアグラウンドで実行
                                    command_status = 1 : バックグラウンドで実行
                                    command_status = 2 : シェルの終了
                                    command_status = 3 : 何もしない */
    char prompt[] = "Command";

    int n = 1;

    DIR_LIST *root;
    root = NULL;

    HISTORY_LIST *top;
    top = NULL;

    ALIAS_LIST *head;
    head = NULL;

    for (;;)
    {

        /*
         *  プロンプトを表示する
         */

        printf("%s : ", prompt);

        /*
         *  標準入力から１行を command_buffer へ読み込む
         *  入力が何もなければ改行を出力してプロンプト表示へ戻る
         */

        if (fgets(command_buffer, BUFLEN, stdin) == NULL)
        {
            cleardirlist(&root);
            clearhislist(&top);
            clearaliaslist(&head);
            exit(EXIT_SUCCESS);
        }

        /*
         *  !! or !strings
         */
        if (strcmp(command_buffer, "!!\n") == 0)
            pre_command(command_buffer, &top);
        else if (command_buffer[0] == '!' && command_buffer[1] >= '0' && command_buffer[1] <= '9' && ((command_buffer[2] >= '0' && command_buffer[2] <= '9') || command_buffer[2] == '\n'))
            norder_command(command_buffer, &top);
        else if (command_buffer[0] == '!' && command_buffer[1] == '-' && command_buffer[2] >= '0' && command_buffer[2] <= '9')
            npre_command(command_buffer, &top);
        else if (command_buffer[0] == '!' && command_buffer[1] != '!' && command_buffer[1] != '\n')
            string_command(command_buffer, &top);

        /*
         *  コマンドを保存
         */
        if (command_buffer[0] == '!' && command_buffer[1] != '\n')
            n--;
        else
            memory_command(&top, command_buffer, n);

        /*
         *  aliasの確認
         */
        aliascheck(command_buffer, &head);

        /*
         *  入力されたバッファ内のコマンドを解析する
         *  返り値はコマンドの状態
         */

        command_status = parse(command_buffer, args);

        /*
         *  終了コマンドならばプログラムを終了
         *  引数が何もなければプロンプト表示へ戻る
         */

        if (command_status == 2)
        {
            cleardirlist(&root);
            clearhislist(&top);
            clearaliaslist(&head);
            printf("done.\n");
            exit(EXIT_SUCCESS);
        }
        else if (command_status == 3)
        {
            continue;
        }

        /*
         *  ワイルドカード処理
         */
        wildcard_command(args);

        /*
         *  コマンドを実行する
         */
        if (strcmp(args[0], "cd") == 0)
            cd_command(args);
        else if (strcmp(args[0], "pushd") == 0)
            pushd_command(&root);
        else if (strcmp(args[0], "prompt") == 0)
            prompt_command(prompt, args);
        else if (strcmp(args[0], "alias") == 0)
            alias_command(args, &head);
        else if (strcmp(args[0], "unalias") == 0)
            unalias_command(args, &head);
        else if (strcmp(args[0], "popd") == 0)
            popd_command(&root);
        else if (strcmp(args[0], "dirs") == 0)
            dirs_command(&root);
        else if (strcmp(args[0], "history") == 0)
            history_command(&top);
        else if (strcmp(args[0], "grep") == 0)
            grep_command(args);
        else if (strcmp(args[0], "cat") == 0)
            cat_command(args);
        else if (strcmp(args[0], "!!") == 0)
            ;
        else if (command_buffer[0] == '!')
            ;
        else
            execute_command(args, command_status, &root, &top);
        n++;
    }
    return 0;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   : parse
 *
 *  作業内容 : バッファ内のコマンドと引数を解析する
 *
 *  引数     :
 *
 *  返り値   : コマンドの状態を表す :
 *                0 : フォアグラウンドで実行
 *                1 : バックグラウンドで実行
 *                2 : シェルの終了
 *                3 : 何もしない
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

int parse(char buffer[], /* バッファ */
          char *args[])  /* 引数へのポインタ配列 */
{
    int arg_index; /* 引数用のインデックス */
    int status;    /* コマンドの状態を表す */

    /*
     *  変数の初期化
     */

    arg_index = 0;
    status = 0;

    /*
     *  バッファ内の最後にある改行をヌル文字へ変更
     */

    if (*(buffer + (strlen(buffer) - 1)) == '\n')
        *(buffer + (strlen(buffer) - 1)) = '\0';

    /*
     *  バッファが終了を表すコマンド（"exit"）ならば
     *  コマンドの状態を表す返り値を 2 に設定してリターンする
     */

    if (strcmp(buffer, "exit") == 0)
    {

        status = 2;
        return status;
    }

    /*
     *  バッファ内の文字がなくなるまで繰り返す
     *  （ヌル文字が出てくるまで繰り返す）
     */

    while (*buffer != '\0')
    {

        /*
         *  空白類（空白とタブ）をヌル文字に置き換える
         *  これによってバッファ内の各引数が分割される
         */

        while (*buffer == ' ' || *buffer == '\t')
        {
            *(buffer++) = '\0';
        }

        /*
         * 空白の後が終端文字であればループを抜ける
         */

        if (*buffer == '\0')
        {
            break;
        }

        /*
         *  空白部分は読み飛ばされたはず
         *  buffer は現在は arg_index + 1 個めの引数の先頭を指している
         *
         *  引数の先頭へのポインタを引数へのポインタ配列に格納する
         */

        args[arg_index] = buffer;
        ++arg_index;

        /*
         *  引数部分を読み飛ばす
         *  （ヌル文字でも空白類でもない場合に読み進める）
         */

        while ((*buffer != '\0') && (*buffer != ' ') && (*buffer != '\t'))
        {
            ++buffer;
        }
    }

    /*
     *  最後の引数の次にはヌルへのポインタを格納する
     */

    args[arg_index] = NULL;

    /*
     *  最後の引数をチェックして "&" ならば
     *
     *  "&" を引数から削る
     *  コマンドの状態を表す status に 1 を設定する
     *
     *  そうでなければ status に 0 を設定する
     */

    if (arg_index > 0 && strcmp(args[arg_index - 1], "&") == 0)
    {

        --arg_index;
        args[arg_index] = '\0';
        status = 1;
    }
    else
    {

        status = 0;
    }

    /*
     *  引数が何もなかった場合
     */

    if (arg_index == 0)
    {
        status = 3;
    }

    /*
     *  コマンドの状態を返す
     */

    return status;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   : execute_command
 *
 *  作業内容 : 引数として与えられたコマンドを実行する
 *             コマンドの状態がフォアグラウンドならば、コマンドを
 *             実行している子プロセスの終了を待つ
 *             バックグラウンドならば子プロセスの終了を待たずに
 *             main 関数に返る（プロンプト表示に戻る）
 *
 *  引数     :
 *
 *  返り値   :
 *
 *  注意     :
 *
 *--------------------------------------------------------------------------*/

void execute_command(char *args[],                                            /* 引数の配列 */
                     int command_status, DIR_LIST **root, HISTORY_LIST **top) /* コマンドの状態 */
{
    int pid;    /* プロセスＩＤ */
    int status; /* 子プロセスの終了ステータス */

    /*
     *  子プロセスを生成する
     *  生成できたかを確認し、失敗ならばプログラムを終了する
     */

    if ((pid = fork()) == -1)
    {
        perror("fork");
        exit(-1);
    }

    /*
     *  子プロセスの場合には引数として与えられたものを実行する
     *
     *  引数の配列は以下を仮定している
     *  ・第１引数には実行されるプログラムを示す文字列が格納されている
     *  ・引数の配列はヌルポインタで終了している
     */

    if (pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            perror("execvp");
        }
    }
    /*
     *  コマンドの状態がバックグラウンドなら関数を抜ける
     */
    if (command_status == 1)
        return;

    /*
     *  ここにくるのはコマンドの状態がフォアグラウンドの場合
     *  親プロセスの場合に子プロセスの終了を待つ
     */
    if (pid > 0)
        wait(&status);

    return;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   : cd_command
 *
 *--------------------------------------------------------------------------*/

void cd_command(char *args[])
{
    char path[256];
    char *check = args[1];
    int i, j, l, p = 0;

    if (args[1] != NULL && args[2] != NULL)
    {
        printf("usage : cd [path]\n");
        return;
    }
    else if (args[1] == NULL)
        strcpy(path, getenv("HOME"));
    else if (check[0] == '.' || check[0] == '~')
    {

        if (check[0] == '.')
        {
            getcwd(path, 256);
            if (check[1] == '.')
            {
                i = 0;
                while (check[i])
                {
                    if (check[i] == '.' && check[i + 1] == '.')
                        p++;
                    i++;
                }
                for (j = 1; j <= 3 * p - 2; j++)
                {
                    i = 0;
                    while (check[i])
                    {
                        check[i] = check[i + 1];
                        i++;
                    }
                }
                for (j = 1; j <= p; j++)
                {
                    if (strcmp(path, "/") != 0)
                    {
                        int tmp = 0;
                        l = 1;
                        while (path[l] != '\0')
                        {
                            if (path[l] == '/')
                                tmp = l;
                            l++;
                        }
                        path[tmp] = '\0';
                    }
                }
            }
        }
        else if (check[0] == '~')
            strcpy(path, getenv("HOME"));
        i = 0;
        while (check[i])
        {
            check[i] = check[i + 1];
            i++;
        }
        strcat(path, check);
    }
    else
        strcpy(path, args[1]);
    if (chdir(path) == -1)
        perror("chdir");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :pushd_command
 *
 *--------------------------------------------------------------------------*/

void pushd_command(DIR_LIST **root)
{
    DIR_LIST *new;
    new = (DIR_LIST *)malloc(sizeof(DIR_LIST));

    if (!new)
    {
        perror("malloc");
        exit(1);
    }

    getcwd(new->dir_name, 256);
    printf("pushd : %s\n", new->dir_name);
    new->next = *root;
    *root = new;
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :dirs_command
 *
 *--------------------------------------------------------------------------*/

void dirs_command(DIR_LIST **root)
{
    if ((*root) == NULL)
    {
        printf("the stack of directories is empty\n");
        return;
    }
    DIR_LIST *p;
    for (p = *root; p != NULL; p = p->next)
        printf("%s\n", p->dir_name);
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :popd_command
 *
 *--------------------------------------------------------------------------*/

void popd_command(DIR_LIST **root)
{
    DIR_LIST *p;
    if (*root != NULL)
    {
        if (chdir((*root)->dir_name) == -1)
            perror("chdir");
        else
        {
            printf("popd : %s\n", (*root)->dir_name);
            p = *root;
            *root = p->next;
            free(p);
        }
    }
    else
        printf("the stack of directories is empty\n");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :memory_command
 *
 *--------------------------------------------------------------------------*/

void memory_command(HISTORY_LIST **top, char command_buffer[], int n)
{
    int l = 1;
    if (strcmp(command_buffer, "\n") == 0)
        return;
    HISTORY_LIST *new, *p;
    new = (HISTORY_LIST *)malloc(sizeof(HISTORY_LIST));
    strcpy(new->command_name, command_buffer);
    new->order = n;
    *(new->command_name + (strlen(new->command_name) - 1)) = '\0';
    new->next = NULL;

    if (*top == NULL)
    {
        *top = new;
    }
    else
    {
        for (p = *top; p->next != NULL; p = p->next)
            l++;
        p->next = new;
        if (l >= 32)
        {
            p = *top;
            *top = p->next;
            free(p);
        }
    }
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :history_command
 *
 *--------------------------------------------------------------------------*/

void history_command(HISTORY_LIST **top)
{

    if ((*top) == NULL)
    {
        printf("history list is empty\n");
        return;
    }
    HISTORY_LIST *p;
    for (p = *top; p != NULL; p = p->next)
        printf("%2d  %s\n", p->order, p->command_name);
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :pre_command
 *
 *--------------------------------------------------------------------------*/

void pre_command(char command_buffer[], HISTORY_LIST **top)
{
    if (*top == NULL)
    {
        printf("even not found\n");
        return;
    }
    HISTORY_LIST *p;
    for (p = *top; p->next != NULL; p = p->next)
        ;
    strcpy(command_buffer, p->command_name);
    strcat(command_buffer, "\n");
    printf("%s", command_buffer);
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :norder_command
 *
 *--------------------------------------------------------------------------*/

void norder_command(char command_buffer[], HISTORY_LIST **top)
{
    if (*top == NULL)
    {
        printf("even not found\n");
        return;
    }
    int i;
    for (i = 0; command_buffer[i + 1] != '\0'; i++)
        command_buffer[i] = command_buffer[i + 1];
    int n = atoi(command_buffer);
    HISTORY_LIST *p;
    for (p = *top; p != NULL; p = p->next)
    {
        if (n == p->order)
        {
            strcpy(command_buffer, p->command_name);
            strcat(command_buffer, "\n");
            return;
        }
    }

    printf("even not found\n");
    strcpy(command_buffer, "!!!");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :npre_command
 *
 *--------------------------------------------------------------------------*/

void npre_command(char command_buffer[], HISTORY_LIST **top)
{
    if (*top == NULL)
    {
        printf("even not found\n");
        return;
    }
    int i, l;
    for (l = 0; l < 2; l++)
    {
        for (i = 0; command_buffer[i + 1] != '\0'; i++)
            command_buffer[i] = command_buffer[i + 1];
    }
    int n = atoi(command_buffer);
    int list = 0;
    HISTORY_LIST *p;
    for (p = *top; p != NULL; p = p->next)
        list++;

    int position = list - n + 1;
    for (p = *top; p != NULL; p = p->next)
    {
        if (position == p->order)
        {
            strcpy(command_buffer, p->command_name);
            strcat(command_buffer, "\n");
            return;
        }
    }

    printf("even not found\n");
    strcpy(command_buffer, "!!!");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :string_command
 *
 *--------------------------------------------------------------------------*/

void string_command(char command_buffer[], HISTORY_LIST **top)
{
    char *command = NULL, *tmp;
    HISTORY_LIST *p;
    int i;

    for (p = *top; p != NULL; p = p->next)
    {
        tmp = p->command_name;

        for (i = 1; i <= strlen(tmp); i++)
        {
            if (command_buffer[i] != tmp[i - 1])
                break;
            else if (command_buffer[i + 1] == '\n')
            {
                command = tmp;
                break;
            }
        }
    }

    if (command == NULL)
    {
        *(command_buffer + (strlen(command_buffer) - 1)) = '\0';
        printf("%s : even not found\n", command_buffer);
        strcat(command_buffer, "\n");
    }
    else
    {
        strcpy(command_buffer, command);
        strcat(command_buffer, "\n");
        printf("%s", command_buffer);
    }
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :wildcard_command
 *
 *-------------------------------------------------------------------*/

void wildcard_command(char *args[])
{
    DIR *dp;
    struct dirent *entry;
    struct stat st;
    char path[512] = "./";
    int position = -1;

    if ((dp = opendir(path)) == NULL)
    {
        perror("opendir");
        return;
    }

    int i, l;
    for (i = 0; args[i] != NULL; i++)
    {
        if (*args[i] == '*')
        {
            position = i;
            while ((entry = readdir(dp)) != NULL)
            {
                path[2] = '\0';
                strcat(path, entry->d_name);
                stat(path, &st);

                if (!S_ISDIR(st.st_mode))
                {
                    int j = i;
                    while (args[j] != NULL)
                        j++;

                    while (j >= i)
                    {
                        args[j + 1] = args[j];
                        j = j - 1;
                    }
                    j++;
                    args[j] = entry->d_name;
                }
            }
        }
    }
    if (position >= 0)
        for (l = position; args[l] != NULL; l++)
            args[l] = args[l + 1];
    closedir(dp);
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :prompt_command
 *
 *--------------------------------------------------------------------------*/

void prompt_command(char prompt[], char *args[])
{
    if (args[1] == NULL)
        strcpy(prompt, "Command");
    else
    {
        int i = 1;
        while (args[i] != NULL)
        {
            if (i == 1)
                strcpy(prompt, args[1]);
            else
                sprintf(prompt, "%s %s", prompt, args[i]);
            i++;
        }
    }
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :alias_command
 *
 *--------------------------------------------------------------------------*/

void alias_command(char *args[], ALIAS_LIST **head)
{
    ALIAS_LIST *p;

    if (args[1] == NULL)
    {
        if ((*head) != NULL)
            for (p = *head; p != NULL; p = p->next)
                printf("%-5s %-5s\n", p->sub, p->original);
    }
    else if (args[1] != NULL && args[2] != NULL)
    {
        ALIAS_LIST *new;
        new = (ALIAS_LIST *)malloc(sizeof(ALIAS_LIST));
        strcpy(new->sub, args[1]);
        strcpy(new->original, args[2]);
        int i = 3;
        while (args[i] != NULL)
        {
            sprintf(new->original, "%s %s", new->original, args[i]);
            i++;
        }

        if (*head == NULL)
        {
            *head = new;
            new->next = NULL;
            printf("[%s %s]  was registerd\n", args[1], new->original);
        }
        else
        {
            for (p = *head; p != NULL; p = p->next)
            {
                if ((strcmp(new->sub, p->sub) == 0) && (strcmp(new->original, p->original) == 0))
                {
                    printf("already registered\n");
                    free(new);
                    return;
                }
                if ((strcmp(new->sub, p->sub) == 0) && (strcmp(new->original, p->original) != 0))
                {
                    printf("[%s %s]  was renewed by [%s %s]\n", args[1], p->original, args[1], new->original);
                    strcpy(p->original, new->original);
                    free(new);
                    return;
                }
            }
            for (p = *head; p->next != NULL; p = p->next)
                ;
            p->next = new;
            new->next = NULL;
            printf("[%s %s]  was registerd\n", args[1], new->original);
        }
    }
    else
        printf("alias usage : alias [command1 command2]\n");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :aliascheck
 *
 *--------------------------------------------------------------------------*/

void aliascheck(char command_buffer[], ALIAS_LIST **head)
{
    *(command_buffer + (strlen(command_buffer) - 1)) = '\0';
    ALIAS_LIST *p;
    for (p = *head; p != NULL; p = p->next)
    {
        if ((strcmp(command_buffer, p->sub) == 0))
        {
            strcpy(command_buffer, p->original);
        }
    }
    strcat(command_buffer, "\n");
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :unalias_command
 *
 *--------------------------------------------------------------------------*/

void unalias_command(char *args[], ALIAS_LIST **head)
{
    if (*head == NULL)
        return;
    if (args[1] == NULL)
    {
        printf("usage : unalias command1\n");
        return;
    }

    ALIAS_LIST *p = *head, *keep;

    if (strcmp(args[1], p->sub) == 0)
    {
        p = *head;
        *head = p->next;
        free(p);
        printf("%s was removed\n", args[1]);
        return;
    }
    else
    {
        for (p = *head; p->next != NULL; p = p->next)
        {
            if (strcmp(args[1], p->next->sub) == 0)
            {
                keep = p->next;
                p->next = keep->next;
                free(keep);
                printf("%s was removed\n", args[1]);
                return;
            }
        }
    }

    printf("%s is not registered\n", args[1]);
}

void cleardirlist(DIR_LIST **root)
{
    DIR_LIST *tmp;
    while ((*root) != NULL)
    {
        tmp = (*root)->next;
        free(*root);
        (*root) = tmp;
    }
}
void clearhislist(HISTORY_LIST **top)
{
    HISTORY_LIST *tmp;
    while ((*top) != NULL)
    {
        tmp = (*top)->next;
        free(*top);
        (*top) = tmp;
    }
}

void clearaliaslist(ALIAS_LIST **head)
{
    ALIAS_LIST *tmp;
    while ((*head) != NULL)
    {
        tmp = (*head)->next;
        free(*head);
        (*head) = tmp;
    }
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :grep_command
 *
 *--------------------------------------------------------------------------*/

void grep_command(char *args[])
{
    if (args[1] == NULL || args[2] == NULL)
    {
        printf("usage : grep [string] [files]\n");
        return;
    }

    FILE *fp;
    char buf[256];
    int cf = 2, i;

    while (args[cf] != NULL)
    {
        fp = fopen(args[cf], "r");
        if (fp == NULL)
        {
            printf("%s is no found\n", args[cf]);
            cf++;
            continue;
        }
        else
        {
            i = 1;
            while (fgets(buf, 256, fp) != NULL)
            {
                if (strstr(buf, args[1]) != NULL)
                    printf("%s : %d : %s", args[cf], i, buf);
                i++;
            }
        }
        fclose(fp);
        cf++;
    }
}

/*----------------------------------------------------------------------------
 *
 *  関数名   :cat_command
 *
 *--------------------------------------------------------------------------*/

void cat_command(char *args[])
{
    if (args[1] == NULL)
    {
        printf("usage : cat [files]\n");
        return;
    }

    FILE *fp;
    char buf[256];
    int cf = 1;

    while (args[cf] != NULL)
    {
        fp = fopen(args[cf], "r");
        if (fp == NULL)
        {
            printf("%s is no found\n", args[cf]);
            cf++;
            continue;
        }
        else
        {
            while (fgets(buf, 256, fp) != NULL)
            {
                printf("%s", buf);
            }
        }
        fclose(fp);
        cf++;
    }
}

/*-- END OF FILE -----------------------------------------------------------*/