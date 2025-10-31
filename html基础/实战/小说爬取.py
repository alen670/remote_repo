import requests
from lxml import etree
from urllib.parse import urljoin
import time
import random
from tenacity import *

# 起始URL
url = "https://www.shuzhaige.com/douluodalu/91693.html"

# 设置请求头，模拟浏览器访问
headers = {
    'User-Agent': 'Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Mobile Safari/537.36 Edg/140.0.0.0'
}

# 计数器和最大章节限制（防止无限循环）
chapter_count = 0
max_chapters = 336  # 你可以根据需要调整这个值

while url and chapter_count < max_chapters:
    try:
        # 发送请求
        resp = requests.get(url, headers=headers, timeout=30)
        resp.encoding = 'utf-8'
        
        # 检查请求是否成功
        if resp.status_code != 200:
            print(f"请求失败，状态码: {resp.status_code}")
            break

        # 解析HTML
        html = etree.HTML(resp.text)
        
        # 提取章节标题
        title_elements = html.xpath('//div[@class="m-title col-md-12"]/h1/text()')
        title = title_elements[0] if title_elements else "未知标题"
        print(f"正在爬取: {title}")
        
        # 提取正文内容
        paragraphs_list = html.xpath('//div[@id="content"]/p/text() | //div[@id="content"]/p/span/text()')
        content = '\n'.join(paragraphs_list)
        
        # 保存到文件
        with open('斗罗大陆.txt', 'a', encoding='utf-8') as f:
            f.write(title + '\n\n')
            f.write(content + '\n\n')
            f.write("="*50 + '\n\n')  # 添加分隔线，便于阅读
        
        # 寻找下一章的URL
        next_chapter_links = html.xpath('//a[contains(text(), "下一章")]/@href')
        if next_chapter_links:
            # 获取下一个URL，并确保它是绝对URL
            next_relative_url = next_chapter_links[0]
            url = urljoin(url, next_relative_url)
        else:
            print("已到达最后一章，爬取结束。")
            break
        
        # 增加计数器
        chapter_count += 1
        
        # 随机延时，避免请求过于频繁
        time.sleep(random.uniform(1, 2))
        
    except Exception as e:
        print(f"爬取过程中出现错误: {e}")
        print("当前URL:", url)
        break

print(f"爬取完成，共爬取 {chapter_count} 章。")