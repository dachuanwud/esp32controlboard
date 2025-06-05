# 🔄 Git版本管理

本指南介绍ESP32控制板项目的Git版本控制规范，包括分支管理、提交规范、协作流程和最佳实践。

## 🎯 版本管理目标

- ✅ 规范化的提交历史
- ✅ 清晰的分支管理策略
- ✅ 高效的团队协作流程
- ✅ 可追溯的版本发布
- ✅ 稳定的代码质量保证

## 📋 分支管理策略

### 主要分支
```
main (主分支)
├── develop (开发分支)
├── feature/* (功能分支)
├── hotfix/* (热修复分支)
└── release/* (发布分支)
```

### 分支说明
| 分支类型 | 命名规范 | 用途 | 生命周期 |
|----------|----------|------|----------|
| main | main | 生产环境代码 | 永久 |
| develop | develop | 开发集成分支 | 永久 |
| feature | feature/功能名 | 新功能开发 | 临时 |
| hotfix | hotfix/问题描述 | 紧急修复 | 临时 |
| release | release/版本号 | 版本发布准备 | 临时 |

## 📝 提交规范

### 提交消息格式
```
<类型>(<范围>): <描述>

[可选的正文]

[可选的脚注]
```

### 提交类型
| 类型 | 描述 | 示例 |
|------|------|------|
| feat | 新功能 | `feat(sbus): 添加SBUS协议解析功能` |
| fix | Bug修复 | `fix(wifi): 修复WiFi重连失败问题` |
| docs | 文档更新 | `docs(readme): 更新安装说明` |
| style | 代码格式 | `style(main): 统一代码缩进格式` |
| refactor | 代码重构 | `refactor(http): 重构HTTP服务器模块` |
| perf | 性能优化 | `perf(sbus): 优化SBUS解析性能` |
| test | 测试相关 | `test(can): 添加CAN通信单元测试` |
| chore | 构建/工具 | `chore(build): 更新编译脚本` |

### 提交示例
```bash
# 功能开发
git commit -m "feat(ota): 实现Web OTA固件更新功能

- 添加HTTP文件上传接口
- 实现双分区OTA机制
- 支持固件完整性验证
- 添加更新进度监控

Closes #123"

# Bug修复
git commit -m "fix(sbus): 修复SBUS数据解析错误

修复在高频数据接收时偶发的帧同步错误，
通过改进状态机逻辑确保数据完整性。

Fixes #456"

# 文档更新
git commit -m "docs(modules): 完善模块文档结构

- 重构文档目录结构
- 添加中文文件名
- 完善模块接口说明
- 添加使用示例"
```

## 🔀 工作流程

### 功能开发流程
```bash
# 1. 从develop分支创建功能分支
git checkout develop
git pull origin develop
git checkout -b feature/sbus-enhancement

# 2. 开发功能
# 编写代码...
git add .
git commit -m "feat(sbus): 添加SBUS信号质量监控"

# 3. 推送到远程仓库
git push origin feature/sbus-enhancement

# 4. 创建Pull Request
# 在GitHub上创建PR，请求合并到develop分支

# 5. 代码审查通过后合并
# 删除功能分支
git checkout develop
git pull origin develop
git branch -d feature/sbus-enhancement
```

### 热修复流程
```bash
# 1. 从main分支创建热修复分支
git checkout main
git pull origin main
git checkout -b hotfix/critical-bug-fix

# 2. 修复问题
git add .
git commit -m "fix(critical): 修复系统崩溃问题"

# 3. 合并到main和develop
git checkout main
git merge hotfix/critical-bug-fix
git push origin main

git checkout develop
git merge hotfix/critical-bug-fix
git push origin develop

# 4. 创建版本标签
git tag -a v1.0.1 -m "Hotfix release v1.0.1"
git push origin v1.0.1

# 5. 删除热修复分支
git branch -d hotfix/critical-bug-fix
```

### 版本发布流程
```bash
# 1. 从develop创建发布分支
git checkout develop
git pull origin develop
git checkout -b release/v1.1.0

# 2. 准备发布
# 更新版本号
# 更新CHANGELOG
# 最后的测试和修复

# 3. 合并到main
git checkout main
git merge release/v1.1.0
git push origin main

# 4. 创建版本标签
git tag -a v1.1.0 -m "Release version 1.1.0"
git push origin v1.1.0

# 5. 合并回develop
git checkout develop
git merge release/v1.1.0
git push origin develop

# 6. 删除发布分支
git branch -d release/v1.1.0
```

## 🏷️ 版本标签管理

### 版本号规范
采用语义化版本控制（Semantic Versioning）：
```
主版本号.次版本号.修订号[-预发布版本][+构建元数据]

例如：
v1.0.0      - 正式版本
v1.1.0-beta - 测试版本
v1.0.1      - 修复版本
```

### 标签创建
```bash
# 创建轻量标签
git tag v1.0.0

# 创建附注标签（推荐）
git tag -a v1.0.0 -m "ESP32控制板 v1.0.0 正式版本

主要功能：
- SBUS遥控信号接收
- CAN电机控制
- Web OTA固件更新
- WiFi网络管理"

# 推送标签到远程
git push origin v1.0.0
git push origin --tags  # 推送所有标签
```

### 标签管理
```bash
# 查看所有标签
git tag

# 查看特定标签信息
git show v1.0.0

# 检出特定版本
git checkout v1.0.0

# 删除本地标签
git tag -d v1.0.0

# 删除远程标签
git push origin --delete v1.0.0
```

## 📁 .gitignore配置

### ESP32项目.gitignore
```gitignore
# ESP-IDF构建输出
build/
sdkconfig.old
dependencies.lock

# IDE文件
.vscode/
.idea/
*.swp
*.swo
*~

# 操作系统文件
.DS_Store
Thumbs.db
desktop.ini

# 临时文件
*.tmp
*.temp
*.log

# 编译产物
*.o
*.a
*.so
*.exe

# 配置文件（包含敏感信息）
config/secrets.h
wifi_credentials.h

# 测试输出
test_results/
coverage/

# 文档生成
docs/_build/
docs/html/

# Node.js（Web客户端）
web_client/node_modules/
web_client/dist/
web_client/.env.local

# Python
__pycache__/
*.pyc
*.pyo
*.pyd
.Python
env/
venv/
```

## 🔍 代码审查流程

### Pull Request模板
```markdown
## 📋 变更描述
简要描述本次变更的内容和目的

## 🎯 变更类型
- [ ] 新功能 (feature)
- [ ] Bug修复 (fix)
- [ ] 文档更新 (docs)
- [ ] 代码重构 (refactor)
- [ ] 性能优化 (perf)
- [ ] 测试相关 (test)

## 🧪 测试情况
- [ ] 单元测试通过
- [ ] 集成测试通过
- [ ] 手动测试完成
- [ ] 性能测试通过

## 📝 检查清单
- [ ] 代码符合编码规范
- [ ] 添加了必要的注释
- [ ] 更新了相关文档
- [ ] 没有引入新的警告
- [ ] 通过了所有测试

## 🔗 相关Issue
Closes #123
Fixes #456
```

### 审查要点
```bash
# 代码质量检查
- 逻辑正确性
- 性能考虑
- 安全性检查
- 错误处理
- 内存管理

# 规范性检查
- 命名规范
- 注释完整性
- 代码格式
- 提交消息规范

# 功能性检查
- 需求实现完整性
- 边界条件处理
- 兼容性考虑
```

## 🛠️ Git配置优化

### 全局配置
```bash
# 用户信息
git config --global user.name "Your Name"
git config --global user.email "your.email@example.com"

# 编辑器
git config --global core.editor "code --wait"

# 默认分支
git config --global init.defaultBranch main

# 自动换行处理
git config --global core.autocrlf input  # Linux/macOS
git config --global core.autocrlf true   # Windows

# 中文文件名支持
git config --global core.quotepath false

# 颜色输出
git config --global color.ui auto
```

### 别名配置
```bash
# 常用别名
git config --global alias.st status
git config --global alias.co checkout
git config --global alias.br branch
git config --global alias.ci commit
git config --global alias.unstage 'reset HEAD --'
git config --global alias.last 'log -1 HEAD'
git config --global alias.visual '!gitk'

# 高级别名
git config --global alias.lg "log --color --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit"
git config --global alias.unstash 'stash pop'
```

## 📊 项目统计和分析

### 提交统计
```bash
# 查看提交历史
git log --oneline --graph --all

# 统计提交数量
git rev-list --count HEAD

# 查看贡献者统计
git shortlog -sn

# 查看文件变更统计
git log --stat

# 查看代码行数统计
git log --pretty=tformat: --numstat | awk '{ add += $1; subs += $2; loc += $1 - $2 } END { printf "added lines: %s, removed lines: %s, total lines: %s\n", add, subs, loc }'
```

### 分支分析
```bash
# 查看分支关系
git log --graph --pretty=oneline --abbrev-commit --all

# 查看未合并的分支
git branch --no-merged

# 查看已合并的分支
git branch --merged
```

## 🚀 最佳实践

### 提交频率
- 🟢 **推荐**: 小而频繁的提交
- 🔴 **避免**: 大而复杂的提交
- 💡 **原则**: 每个提交只做一件事

### 分支管理
- 🟢 **推荐**: 及时删除已合并的分支
- 🟢 **推荐**: 定期同步远程分支
- 🔴 **避免**: 长期存在的功能分支

### 协作规范
- 🟢 **推荐**: 提交前先拉取最新代码
- 🟢 **推荐**: 使用Pull Request进行代码审查
- 🔴 **避免**: 直接推送到主分支

---

💡 **提示**: 良好的版本管理是团队协作的基础，请严格遵守Git规范以确保项目的稳定发展！

🔗 **相关链接**:
- [编码规范指南](编码规范指南.md)
- [调试方法指南](调试方法指南.md)
- [性能优化指南](性能优化指南.md)
