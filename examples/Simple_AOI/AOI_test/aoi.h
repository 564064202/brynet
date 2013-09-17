#ifndef _AOI_H
#define _AOI_H

#define BLOCK_WIDTH (10)		/*	��Ԫ����	*/
#define SCREEN_RADIUS (10)		/*	����Ӿ���Χ	*/
#define MAP_WIDTH (800)			/*	��ͼ���		*/

#define RANGE_BLOCK_NUM ((SCREEN_RADIUS)/BLOCK_WIDTH)  /*   BLOCK����  */

struct mapaoi_obj_s;

struct map_aoiobj_link_node_s
{
	struct double_link_node_s  node;
	struct mapaoi_obj_s* obj;
};

class MapAoi;

class MapAoiBlock
{
public:
	bool    contain(int x, int y);
	int     getRow();
	int     getColumn();

	struct double_link_s*   getObserverList();

private:
	friend class MapAoi;

	void	setPos(int x, int y);
	void    setRowColumn(int row, int column);

private:
	/*  �۲���(֪ͨ)�б�   */
	struct double_link_s    m_observer_list;
	/*	block���Ͻ�����	*/
	int                     m_x,  m_y;
	/*	block�ڵ�ͼ��block�����е����к�	*/
	int                     m_row;
	int                     m_column;
};

#define PLAYER_LINK_NODE_SIZE (2*RANGE_BLOCK_NUM+1)

struct mapaoi_obj_s
{
	char            name[10];

public:
	mapaoi_obj_s();

	int			    getX();
	int			    getY();

	MapAoiBlock*    getMapBlock();

	void		    setXY(int x, int y);

	/*	TODO::��ȡ��������	*/
	bool		    isNotifyed();
	void		    setNotifyed(bool notifyed);

private:
	friend class MapAoi;
	void		    setMapBlock(MapAoiBlock* block);

private:
	int			    m_x;
	int			    m_y;
	bool		    m_notifyed;	/*	�Ƿ�֪ͨ	*/

    /*  ����block */
    MapAoiBlock*    m_block;

    /*  ÿ����ҵ�ע������:���ڽ������ӵ�ĳBLOCK�Ĺ۲����б���ʹ�õ�node   */
    struct map_aoiobj_link_node_s   m_nodes[PLAYER_LINK_NODE_SIZE][PLAYER_LINK_NODE_SIZE];
};

class MapAoi
{
public:
	MapAoi();
	
	/*  ע���ȡ��ע�ᵽ�������λ��BLOCK�Լ�����ΧBLOCKS(�����Ӿ���С):����ֵΪ��ҵ�ǰλ������block    */
	MapAoiBlock*	registerBlock(struct mapaoi_obj_s* player, bool b_enter);
private:
	int			    getBlockIndex(int pos);
	/*  ���������ȡ��Ӧ��BLOCK  */
	MapAoiBlock*    getBlock(int x, int y);

private:
	MapAoiBlock*    m_blocks;
	int             m_side_block_num;   /*  �߳�������block����    */
};

#endif