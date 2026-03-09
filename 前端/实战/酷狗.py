import requests
import os

def download_music(url, save_path):
    """下载音乐文件"""
    try:
        # 设置请求头
        headers = {
            'User-Agent': 'Mozilla/5.0 (Linux; Android 6.0; Nexus 5 Build/MRA58N) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Mobile Safari/537.36 Edg/140.0.0.0'
        }
        
        # 发送请求
        response = requests.get(url, headers=headers, timeout=30)
        response.raise_for_status()  # 检查请求是否成功
        
        # 确保保存目录存在
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        
        # 保存文件
        with open(save_path, 'wb') as f:
            f.write(response.content)
        
        print(f"音乐下载成功: {save_path}")
        return True
        
    except requests.exceptions.RequestException as e:
        print(f"网络请求出错: {e}")
    except IOError as e:
        print(f"文件操作出错: {e}")
    except Exception as e:
        print(f"未知错误: {e}")
    
    return False

# 使用示例
if __name__ == "__main__":
    music_url = "https://m704.music.126.net/20250914022911/30679013e58b55ceb35a4c8f6abfe301/jdyyaac/obj/w5rDlsOJwrLDjj7CmsOj/32285836574/04bb/5c6f/beac/22a7ecfe6a5167a72cffbe47853caf83.m4a?vuutv=jRpKYDnsjiN94f1RLJWglqU4g9nOW6mq9bWpyAk4D9jjF6Tw/oEiUGHqDhKYd5I+wwN36LD/NnqB6COyvbJnJn7eV0PNRlLe3pH5y8J0MLg=&authSecret=00000199443fbaaa06ca0a3b1e10f6cc&cdntag=bWFyaz1vc193ZWIscXVhbGl0eV9leGhpZ2g"  # 替换为实际音乐URL
    save_path = "./实战/paradox.mp3"
    download_music(music_url, save_path)