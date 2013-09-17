#include <iostream>
using namespace std;

#include "double_link.h"
#include "aoi.h"

enum map_obj_type
{
	map_obj_npc,
	map_obj_player,
};

enum send_type
{
	normal_send,
	force_send,
	check_send,
};

static void clean_notifyflag(MapAoiBlock* block)
{
	struct double_link_node_s* begin = double_link_begin(block->getObserverList());
	struct double_link_node_s* end = double_link_end(block->getObserverList());
	while(begin != end)
	{
		struct map_aoiobj_link_node_s* notify_player_node = (struct map_aoiobj_link_node_s*)begin;
		struct mapaoi_obj_s* notify_player = notify_player_node->obj;
		notify_player->setNotifyed(false);

		begin = begin->next;
	}
}

static void send2Block(struct mapaoi_obj_s& player, enum send_type type, MapAoiBlock* block)
{
	struct double_link_node_s* begin = double_link_begin(block->getObserverList());
	struct double_link_node_s* end = double_link_end(block->getObserverList());

	while(begin != end)
	{
		struct map_aoiobj_link_node_s* notify_player_node = (struct map_aoiobj_link_node_s*)begin;
		struct mapaoi_obj_s* notify_player = notify_player_node->obj;

		if(type == normal_send)
		{
			printf("��ͨ����һ��%s��:%s\n", player.name, notify_player->name);
		}
		else if(type == force_send)
		{
			printf("ǿ�Ʒ���һ��%s��:%s\n", player.name, notify_player->name);
			notify_player->setNotifyed(true);
		}
		else if(type == check_send)
		{
			if(!notify_player->isNotifyed())
			{
				printf("��鷢��һ��%s��:%s\n", player.name, notify_player->name);
			}
		}

		begin = begin->next;
	}
}

static void enter_map(MapAoi& map, struct mapaoi_obj_s& player)
{
	MapAoiBlock* ret_block = map.registerBlock(&player, true);
	send2Block(player, normal_send, ret_block);
}

static void leave_map(MapAoi& map, struct mapaoi_obj_s& player)
{
	MapAoiBlock* ret_block = map.registerBlock(&player, false);
	send2Block(player, normal_send, ret_block);
}

static void move_map(MapAoi& map, struct mapaoi_obj_s& player, int dest_x, int dest_y)
{
	MapAoiBlock* ret = player.getMapBlock();
	if(ret == NULL)	return;

	if(!ret->contain(dest_x, dest_y))
	{
		/*  �Ե�ǰ�����Χ��BLOCK����ȡ��ע�� */
		MapAoiBlock* old_block = ret;

		map.registerBlock(&player, false);

		player.setXY(dest_x, dest_y);

		send2Block(player, force_send, old_block);
		/*	���ûص�������־Ϊǿ�Ʒ���	*/

		/*  ������µ�����BLOCK�Լ���Χ��BLOCKS����ע�� (������֮ǰȡ��ע���BLOCK���±�ע��::TODO,���ڹ��ܶ�����Ӱ��) */
		MapAoiBlock* new_block = map.registerBlock(&player, true);

		send2Block(player, check_send, new_block);
		/*  ����ѷ��ͱ�־ */
		clean_notifyflag(old_block);
	}
	else
	{
		player.setXY(dest_x, dest_y);
		send2Block(player, normal_send, ret);
	}
}

int main()
{
    MapAoi m;
    {
        struct mapaoi_obj_s player;
        strncpy_s(player.name, "xiexie", sizeof(player.name));
        player.setXY(102, 102);

		enter_map(m, player);

        move_map(m, player, 121, 121);
    }
    
    {
        struct mapaoi_obj_s player;
        strncpy_s(player.name, "dodo", sizeof(player.name));
        player.setXY(102, 102);

        enter_map(m, player);
        move_map(m, player, 121, 121);
    }

    {
        struct mapaoi_obj_s player;
        strncpy_s(player.name, "irons", sizeof(player.name));
        player.setXY(102, 102);

        enter_map(m, player);
        move_map(m, player, 121, 121);
		move_map(m, player, 102, 102);
		move_map(m, player, 101, 101);
    }

    return 0;
}