http://www.zhihu.com/question/24076731?utm_campaign=rss&utm_medium=rss&utm_source=rss&utm_content=title

vczh，专业造轮子，http://www.gaclib.net
赵老师、赖威、Fel Peter 等人赞同

目前C++语法下面，最好的print应该是长这个样子的

```
class ITextWriter abstract
{
protected:
    virtual void WriteText(wchar_t* text) = 0;

public:
    template<typename ...TArgs>
    ITextWriter& Write(const wstring& pattern, TArgs&& ...args)
    {
        ...
    }

    template<typename ...TArgs>
    ITextWriter& WriteLine(const wstring& pattern, TArgs&& ...args);
    {
        Write(pattern, std::forward<TArgs>(args)...);
        WriteText(L"\r\n");
        return *this;
    }
};
```

用法：

```
Console.Write(L"I am {0}, bitch", L"the CEO");

File(L"C:\Fuckers.txt")
    .WriteLine(L"Ali")
    .WriteLine(L"Baidu")
    .WriteLine(L"G{0}{0}gl{1}", 0, L"e");
```
