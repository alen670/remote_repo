## 📋 日常同步操作总结

### 基本同步流程（三步走）

每次修改文件后，执行以下命令：

```powershell
# 1. 添加所有修改到暂存区
git add .

# 2. 提交更改并添加描述
git commit -m "描述您所做的修改内容"

# 3. 推送到GitHub远程仓库gu
git push origin main
```

### 详细操作步骤

#### 第一步：进入工作目录
```powershell
cd C:\Users\Morta\Desktop\computer\notebook
```

#### 第二步：检查当前状态（可选但推荐）
```powershell
git status
```
这会显示哪些文件被修改、新增或删除。

#### 第三步：执行同步三连击
```powershell
git add .
git commit -m "例如：更新学习笔记内容"
git push origin main
```

## 🔧 实用辅助命令

### 查看提交历史
```powershell
git log --oneline
```

### 查看文件具体修改内容
```powershell
git diff
```

### 如果只想提交特定文件
```powershell
# 提交单个文件
git add 文件名.md
git commit -m "更新特定文件"
git push origin main

# 提交某类文件
git add *.md
```

### 从GitHub拉取最新更改（团队协作时）
```powershell
git pull origin main
```

## 💡 最佳实践建议

### 提交信息规范
使用有意义的提交信息：
- ❌ 不好的：`git commit -m "更新"`
- ✅ 好的：`git commit -m "添加Git学习笔记第三章内容"`

### 频繁提交
建议小步频繁提交，而不是一次性提交大量修改：
```powershell
# 完成一个小功能就提交一次
git add .
git commit -m "完成用户登录功能"
git push origin main
```

## 🚨 常见情况处理

### 如果推送失败（其他人已修改）
```powershell
# 先拉取远程更改
git pull origin main

# 解决可能的冲突后重新推送
git push origin main
```

### 撤销本地修改（如果需要）
```powershell
# 撤销某个文件的修改
git checkout -- 文件名.md

# 撤销所有未提交的修改
git reset --hard HEAD
```

## 📁 您的专属命令备忘单

```powershell
# === 日常同步 ===
cd C:\Users\Morta\Desktop\computer\notebook
git add .
git commit -m "描述修改内容"
git push origin main

# === 检查状态 ===
git status          # 查看修改状态
git log --oneline   # 查看提交历史

# === 问题排查 ===
git pull origin main    # 拉取远程更新
git remote -v          # 检查远程仓库连接
```

## ✅ 验证同步成功

每次推送后，访问您的GitHub仓库页面刷新查看：
- https://github.com/alen670/remote_repo

应该能看到最新的文件修改时间和提交信息。

**您现在只需要记住这三个核心命令：**
```powershell
git add .
git commit -m "您的描述"
git push origin main
```

这就是您日常同步需要的全部操作！有什么问题随时问我。