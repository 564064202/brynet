#ifndef _AOI_H
#define _AOI_H

#include "double_link.h"

#define BLOCK_WIDTH (10)        /*  ��Ԫ����       */
#define SCREEN_RADIUS (30)      /*  ����Ӿ��뾶      */
#define MAP_WIDTH (400)         /*  ��ͼ���        */

#define RANGE_BLOCK_NUM ((SCREEN_RADIUS)/BLOCK_WIDTH)  /*   �Ӿ��뾶�ڵ�Ԫ������  */

struct mapaoi_obj_s;

/*  AOIģ���������ڵ�   */
struct map_aoiobj_link_node_s
{
    struct double_link_node_s  node;
    struct mapaoi_obj_s* obj;
};

class MapAoi;

/*  ��ͼ��Ԫ��   */
class MapAoiBlock
{
public:
    /*  ��������Ƿ����ڴ˵�Ԫ��  */
    bool    contain(int x, int y);
    int     getRow();
    int     getColumn();

    /*  ��ȡ֪ͨ�б� */
    struct double_link_s*   getObserverList();

private:
    friend class MapAoi;

    void    setPos(int x, int y);
    void    setRowColumn(int row, int column);

private:
    /*  �۲���(֪ͨ)�б�   */
    struct double_link_s    m_observer_list;
    /*  ��Ԫ�����Ͻ�����    */
    int                     m_x,  m_y;
    /*  �˵�Ԫ���ڵ�ͼ�ĵ�Ԫ�������е����к�  */
    int                     m_row;
    int                     m_column;
};

#define PLAYER_LINK_NODE_SIZE (2*RANGE_BLOCK_NUM+1)

/*  AOIģ�����Ļ�������    */
struct mapaoi_obj_s
{
public:
    mapaoi_obj_s();

    int             getX() const;
    int             getY() const;

    MapAoiBlock*    getMapBlock();

protected:
    void            setXY(int x, int y);

private:
    friend class MapAoi;
    void            setMapBlock(MapAoiBlock* block);

private:
    int             m_x;
    int             m_y;

    /*  ������Ԫ�� */
    MapAoiBlock*    m_block;

    /*  ÿ����ҵ�ע������:���ڽ������ӵ�ĳ��Ԫ��Ĺ۲����б���ʹ�õ�node   */
    struct map_aoiobj_link_node_s   m_nodes[PLAYER_LINK_NODE_SIZE][PLAYER_LINK_NODE_SIZE];
};

/*  ��ͼAOIģ�� */
class MapAoi
{
public:
    MapAoi();
    ~MapAoi();

    /*  ע���ȡ��ע�ᵽ�������λ�õ�Ԫ���Լ�����Χ��Ԫ��(�����Ӿ���С):����ֵΪ��ҵ�ǰλ��������Ԫ��    */
    MapAoiBlock*    registerBlock(struct mapaoi_obj_s* player, bool b_enter);
    bool            isValid(int x, int y);
private:
    int             getBlockIndex(int pos);
    /*  ���������ȡ��Ӧ�ĵ�Ԫ��    */
    MapAoiBlock*    getBlock(int x, int y);

private:
    MapAoiBlock*    m_blocks;
    int             m_side_block_num;   /*  �߳������ĵ�Ԫ������  */
};

#endif