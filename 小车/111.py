import time
import random

def print_slow(text):
    for char in text:
        print(char, end='', flush=True)
        time.sleep(0.03)
    print()

def start_game():
    print_slow("欢迎来到【神秘洞穴探险】！")
    print_slow("你站在一个幽暗的洞穴入口，冷风从里面吹出……")
    print_slow("前方有两条路：左边潮湿阴暗，右边传来微弱的光。")
    
    choice = input("你要走哪边？(输入 '左' 或 '右')：").strip().lower()
    
    if choice == '左':
        left_path()
    elif choice == '右':
        right_path()
    else:
        print_slow("你犹豫太久，被蝙蝠吓跑了！游戏结束。")
        play_again()

def left_path():
    print_slow("你走进左边的通道，脚下湿滑……")
    print_slow("突然！地面塌陷——你掉进了一个坑里！")
    if random.choice([True, False]):
        print_slow("幸运的是，坑底有个旧梯子，你爬了上来！")
        print_slow("前方出现一个宝箱！")
        open_chest()
    else:
        print_slow("坑太深了……你被困住了。游戏结束。")
        play_again()

def right_path():
    print_slow("你走向右边，发现一盏古老的油灯。")
    print_slow("灯光照亮了一扇石门，上面刻着谜题：")
    print_slow("'我走得越远，留下的越多。我是什么？'")
    answer = input("请输入你的答案：").strip().lower()
    
    if '脚印' in answer or '足迹' in answer:
        print_slow("石门缓缓打开！里面是闪闪发光的宝藏！🎉")
        print_slow("你赢了！")
    else:
        print_slow("石门发出轰鸣，开始崩塌！你逃了出来，但空手而归。")
    play_again()

def open_chest():
    print_slow("你慢慢打开宝箱……")
    time.sleep(1)
    if random.choice([True, False]):
        print_slow("金光四射！里面全是金币和宝石！你发财了！💰")
    else:
        print_slow("砰！宝箱是陷阱，喷出一团烟雾……你晕了过去。")
        print_slow("醒来时，你已在洞外，身无分文。")
    play_again()

def play_again():
    again = input("\n想再玩一次吗？(y/n)：").strip().lower()
    if again == 'y':
        print("\n" + "="*40 + "\n")
        start_game()
    else:
        print_slow("感谢游玩！再见！👋")

# 启动游戏
if __name__ == "__main__":
    start_game()