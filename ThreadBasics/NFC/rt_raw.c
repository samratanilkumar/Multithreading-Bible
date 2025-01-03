/*
 * =====================================================================================
 *
 *       Filename:  rt.h
 *
 *    Description:  
 *
 
 *                                  
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory.h>
#include <assert.h>
#include "rt_raw.h"

void
rt_init_rt_table(rt_table_t *rt_table){

    rt_table->head = NULL;
}

rt_entry_t *
rt_add_or_update_rt_entry(rt_table_t *rt_table,
                    char *dest, 
                    char mask, 
                    char *gw_ip, 
                    char *oif){

	bool new_entry;
    rt_entry_t *head = NULL;
    rt_entry_t *rt_entry = NULL;	

	new_entry = false;
	rt_entry = rt_look_up_rt_entry(rt_table, dest, mask);

	if(!rt_entry) {
    	
		rt_entry = calloc(1, sizeof(rt_entry_t));
    	
		strncpy(rt_entry->rt_entry_keys.dest, dest,
			sizeof(rt_entry->rt_entry_keys.dest));
    	rt_entry->rt_entry_keys.mask = mask;
		new_entry = true;
	}

    if(gw_ip)
        strncpy(rt_entry->gw_ip, gw_ip, sizeof(rt_entry->gw_ip));
    if(oif)
        strncpy(rt_entry->oif, oif, sizeof(rt_entry->oif));
	
	if (new_entry) {
		head = rt_table->head;
		rt_table->head = rt_entry;
		rt_entry->prev = 0;
		rt_entry->next = head;
		if(head)
			head->prev = rt_entry;
	}

    return rt_entry;
}

bool
rt_delete_rt_entry(rt_table_t *rt_table,
    char *dest, char mask){

    rt_entry_t *rt_entry = NULL;

    ITERTAE_RT_TABLE_BEGIN(rt_table, rt_entry){
    
        if(strncmp(rt_entry->rt_entry_keys.dest, 
            dest, sizeof(rt_entry->rt_entry_keys.dest)) == 0 &&
            rt_entry->rt_entry_keys.mask == mask){

            rt_entry_remove(rt_table, rt_entry);
            free(rt_entry);
            return true;
        }
    } ITERTAE_RT_TABLE_END(rt_table, curr);

    return false;
}

void
rt_clear_rt_table(rt_table_t *rt_table){


}

void
rt_free_rt_table(rt_table_t *rt_table){


}

void
rt_dump_rt_table(rt_table_t *rt_table){

    rt_entry_t *rt_entry = NULL;

    ITERTAE_RT_TABLE_BEGIN(rt_table, rt_entry){

        printf("%-20s %-4d %-20s %s\n",
            rt_entry->rt_entry_keys.dest, 
            rt_entry->rt_entry_keys.mask, 
            rt_entry->gw_ip,
            rt_entry->oif);
		printf("\tPrinting Subscribers : ");
		
    } ITERTAE_RT_TABLE_END(rt_table, rt_entry);
}

rt_entry_t *
rt_look_up_rt_entry(rt_table_t *rt_table,
					char *dest, char mask) {

	rt_entry_t *rt_entry = NULL;
	
	ITERTAE_RT_TABLE_BEGIN(rt_table, rt_entry) {

		if ((strncmp(rt_entry->rt_entry_keys.dest,
					dest, sizeof(rt_entry->rt_entry_keys.dest)) == 0) &&
			rt_entry->rt_entry_keys.mask == mask) {

			return rt_entry;
		}
	} ITERTAE_RT_TABLE_END(rt_table, rt_entry);
	return NULL;
}

