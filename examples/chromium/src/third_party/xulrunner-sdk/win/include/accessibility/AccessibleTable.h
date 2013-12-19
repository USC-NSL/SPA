

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0499 */
/* at Mon Dec 01 09:02:22 2008
 */
/* Compiler settings for e:/builds/tinderbox/XR-Trunk/WINNT_5.2_Depend/mozilla/other-licenses/ia2/AccessibleTable.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, app_config, c_ext
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 440
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __AccessibleTable_h__
#define __AccessibleTable_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IAccessibleTable_FWD_DEFINED__
#define __IAccessibleTable_FWD_DEFINED__
typedef interface IAccessibleTable IAccessibleTable;
#endif 	/* __IAccessibleTable_FWD_DEFINED__ */


/* header files for imported files */
#include "objidl.h"
#include "oaidl.h"
#include "oleacc.h"
#include "Accessible2.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_AccessibleTable_0000_0000 */
/* [local] */ 


enum IA2TableModelChangeType
    {	IA2_TABLE_MODEL_CHANGE_INSERT	= 0,
	IA2_TABLE_MODEL_CHANGE_DELETE	= ( IA2_TABLE_MODEL_CHANGE_INSERT + 1 ) ,
	IA2_TABLE_MODEL_CHANGE_UPDATE	= ( IA2_TABLE_MODEL_CHANGE_DELETE + 1 ) 
    } ;
typedef /* [public][public] */ struct __MIDL___MIDL_itf_AccessibleTable_0000_0000_0001
    {
    enum IA2TableModelChangeType type;
    long firstRow;
    long lastRow;
    long firstColumn;
    long lastColumn;
    } 	IA2TableModelChange;



extern RPC_IF_HANDLE __MIDL_itf_AccessibleTable_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_AccessibleTable_0000_0000_v0_0_s_ifspec;

#ifndef __IAccessibleTable_INTERFACE_DEFINED__
#define __IAccessibleTable_INTERFACE_DEFINED__

/* interface IAccessibleTable */
/* [uuid][object] */ 


EXTERN_C const IID IID_IAccessibleTable;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("35AD8070-C20C-4fb4-B094-F4F7275DD469")
    IAccessibleTable : public IUnknown
    {
    public:
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_accessibleAt( 
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ IUnknown **accessible) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_caption( 
            /* [retval][out] */ IUnknown **accessible) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_childIndex( 
            /* [in] */ long rowIndex,
            /* [in] */ long columnIndex,
            /* [retval][out] */ long *childIndex) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnDescription( 
            /* [in] */ long column,
            /* [retval][out] */ BSTR *description) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnExtentAt( 
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ long *nColumnsSpanned) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnHeader( 
            /* [out] */ IAccessibleTable **accessibleTable,
            /* [retval][out] */ long *startingRowIndex) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_columnIndex( 
            /* [in] */ long childIndex,
            /* [retval][out] */ long *columnIndex) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nColumns( 
            /* [retval][out] */ long *columnCount) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nRows( 
            /* [retval][out] */ long *rowCount) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedChildren( 
            /* [retval][out] */ long *childCount) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedColumns( 
            /* [retval][out] */ long *columnCount) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_nSelectedRows( 
            /* [retval][out] */ long *rowCount) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowDescription( 
            /* [in] */ long row,
            /* [retval][out] */ BSTR *description) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowExtentAt( 
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ long *nRowsSpanned) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowHeader( 
            /* [out] */ IAccessibleTable **accessibleTable,
            /* [retval][out] */ long *startingColumnIndex) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowIndex( 
            /* [in] */ long childIndex,
            /* [retval][out] */ long *rowIndex) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedChildren( 
            /* [in] */ long maxChildren,
            /* [length_is][length_is][size_is][size_is][out] */ long **children,
            /* [retval][out] */ long *nChildren) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedColumns( 
            /* [in] */ long maxColumns,
            /* [length_is][length_is][size_is][size_is][out] */ long **columns,
            /* [retval][out] */ long *nColumns) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_selectedRows( 
            /* [in] */ long maxRows,
            /* [length_is][length_is][size_is][size_is][out] */ long **rows,
            /* [retval][out] */ long *nRows) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_summary( 
            /* [retval][out] */ IUnknown **accessible) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isColumnSelected( 
            /* [in] */ long column,
            /* [retval][out] */ boolean *isSelected) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isRowSelected( 
            /* [in] */ long row,
            /* [retval][out] */ boolean *isSelected) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_isSelected( 
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ boolean *isSelected) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE selectRow( 
            /* [in] */ long row) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE selectColumn( 
            /* [in] */ long column) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE unselectRow( 
            /* [in] */ long row) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE unselectColumn( 
            /* [in] */ long column) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_rowColumnExtentsAtIndex( 
            /* [in] */ long index,
            /* [out] */ long *row,
            /* [out] */ long *column,
            /* [out] */ long *rowExtents,
            /* [out] */ long *columnExtents,
            /* [retval][out] */ boolean *isSelected) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_modelChange( 
            /* [retval][out] */ IA2TableModelChange *modelChange) = 0;
        
    };
    
#else 	/* C style interface */

    typedef struct IAccessibleTableVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IAccessibleTable * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IAccessibleTable * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IAccessibleTable * This);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_accessibleAt )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ IUnknown **accessible);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_caption )( 
            IAccessibleTable * This,
            /* [retval][out] */ IUnknown **accessible);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_childIndex )( 
            IAccessibleTable * This,
            /* [in] */ long rowIndex,
            /* [in] */ long columnIndex,
            /* [retval][out] */ long *childIndex);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_columnDescription )( 
            IAccessibleTable * This,
            /* [in] */ long column,
            /* [retval][out] */ BSTR *description);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_columnExtentAt )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ long *nColumnsSpanned);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_columnHeader )( 
            IAccessibleTable * This,
            /* [out] */ IAccessibleTable **accessibleTable,
            /* [retval][out] */ long *startingRowIndex);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_columnIndex )( 
            IAccessibleTable * This,
            /* [in] */ long childIndex,
            /* [retval][out] */ long *columnIndex);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nColumns )( 
            IAccessibleTable * This,
            /* [retval][out] */ long *columnCount);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nRows )( 
            IAccessibleTable * This,
            /* [retval][out] */ long *rowCount);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nSelectedChildren )( 
            IAccessibleTable * This,
            /* [retval][out] */ long *childCount);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nSelectedColumns )( 
            IAccessibleTable * This,
            /* [retval][out] */ long *columnCount);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_nSelectedRows )( 
            IAccessibleTable * This,
            /* [retval][out] */ long *rowCount);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_rowDescription )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [retval][out] */ BSTR *description);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_rowExtentAt )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ long *nRowsSpanned);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_rowHeader )( 
            IAccessibleTable * This,
            /* [out] */ IAccessibleTable **accessibleTable,
            /* [retval][out] */ long *startingColumnIndex);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_rowIndex )( 
            IAccessibleTable * This,
            /* [in] */ long childIndex,
            /* [retval][out] */ long *rowIndex);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_selectedChildren )( 
            IAccessibleTable * This,
            /* [in] */ long maxChildren,
            /* [length_is][length_is][size_is][size_is][out] */ long **children,
            /* [retval][out] */ long *nChildren);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_selectedColumns )( 
            IAccessibleTable * This,
            /* [in] */ long maxColumns,
            /* [length_is][length_is][size_is][size_is][out] */ long **columns,
            /* [retval][out] */ long *nColumns);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_selectedRows )( 
            IAccessibleTable * This,
            /* [in] */ long maxRows,
            /* [length_is][length_is][size_is][size_is][out] */ long **rows,
            /* [retval][out] */ long *nRows);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_summary )( 
            IAccessibleTable * This,
            /* [retval][out] */ IUnknown **accessible);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isColumnSelected )( 
            IAccessibleTable * This,
            /* [in] */ long column,
            /* [retval][out] */ boolean *isSelected);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isRowSelected )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [retval][out] */ boolean *isSelected);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_isSelected )( 
            IAccessibleTable * This,
            /* [in] */ long row,
            /* [in] */ long column,
            /* [retval][out] */ boolean *isSelected);
        
        HRESULT ( STDMETHODCALLTYPE *selectRow )( 
            IAccessibleTable * This,
            /* [in] */ long row);
        
        HRESULT ( STDMETHODCALLTYPE *selectColumn )( 
            IAccessibleTable * This,
            /* [in] */ long column);
        
        HRESULT ( STDMETHODCALLTYPE *unselectRow )( 
            IAccessibleTable * This,
            /* [in] */ long row);
        
        HRESULT ( STDMETHODCALLTYPE *unselectColumn )( 
            IAccessibleTable * This,
            /* [in] */ long column);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_rowColumnExtentsAtIndex )( 
            IAccessibleTable * This,
            /* [in] */ long index,
            /* [out] */ long *row,
            /* [out] */ long *column,
            /* [out] */ long *rowExtents,
            /* [out] */ long *columnExtents,
            /* [retval][out] */ boolean *isSelected);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_modelChange )( 
            IAccessibleTable * This,
            /* [retval][out] */ IA2TableModelChange *modelChange);
        
        END_INTERFACE
    } IAccessibleTableVtbl;

    interface IAccessibleTable
    {
        CONST_VTBL struct IAccessibleTableVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IAccessibleTable_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IAccessibleTable_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IAccessibleTable_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IAccessibleTable_get_accessibleAt(This,row,column,accessible)	\
    ( (This)->lpVtbl -> get_accessibleAt(This,row,column,accessible) ) 

#define IAccessibleTable_get_caption(This,accessible)	\
    ( (This)->lpVtbl -> get_caption(This,accessible) ) 

#define IAccessibleTable_get_childIndex(This,rowIndex,columnIndex,childIndex)	\
    ( (This)->lpVtbl -> get_childIndex(This,rowIndex,columnIndex,childIndex) ) 

#define IAccessibleTable_get_columnDescription(This,column,description)	\
    ( (This)->lpVtbl -> get_columnDescription(This,column,description) ) 

#define IAccessibleTable_get_columnExtentAt(This,row,column,nColumnsSpanned)	\
    ( (This)->lpVtbl -> get_columnExtentAt(This,row,column,nColumnsSpanned) ) 

#define IAccessibleTable_get_columnHeader(This,accessibleTable,startingRowIndex)	\
    ( (This)->lpVtbl -> get_columnHeader(This,accessibleTable,startingRowIndex) ) 

#define IAccessibleTable_get_columnIndex(This,childIndex,columnIndex)	\
    ( (This)->lpVtbl -> get_columnIndex(This,childIndex,columnIndex) ) 

#define IAccessibleTable_get_nColumns(This,columnCount)	\
    ( (This)->lpVtbl -> get_nColumns(This,columnCount) ) 

#define IAccessibleTable_get_nRows(This,rowCount)	\
    ( (This)->lpVtbl -> get_nRows(This,rowCount) ) 

#define IAccessibleTable_get_nSelectedChildren(This,childCount)	\
    ( (This)->lpVtbl -> get_nSelectedChildren(This,childCount) ) 

#define IAccessibleTable_get_nSelectedColumns(This,columnCount)	\
    ( (This)->lpVtbl -> get_nSelectedColumns(This,columnCount) ) 

#define IAccessibleTable_get_nSelectedRows(This,rowCount)	\
    ( (This)->lpVtbl -> get_nSelectedRows(This,rowCount) ) 

#define IAccessibleTable_get_rowDescription(This,row,description)	\
    ( (This)->lpVtbl -> get_rowDescription(This,row,description) ) 

#define IAccessibleTable_get_rowExtentAt(This,row,column,nRowsSpanned)	\
    ( (This)->lpVtbl -> get_rowExtentAt(This,row,column,nRowsSpanned) ) 

#define IAccessibleTable_get_rowHeader(This,accessibleTable,startingColumnIndex)	\
    ( (This)->lpVtbl -> get_rowHeader(This,accessibleTable,startingColumnIndex) ) 

#define IAccessibleTable_get_rowIndex(This,childIndex,rowIndex)	\
    ( (This)->lpVtbl -> get_rowIndex(This,childIndex,rowIndex) ) 

#define IAccessibleTable_get_selectedChildren(This,maxChildren,children,nChildren)	\
    ( (This)->lpVtbl -> get_selectedChildren(This,maxChildren,children,nChildren) ) 

#define IAccessibleTable_get_selectedColumns(This,maxColumns,columns,nColumns)	\
    ( (This)->lpVtbl -> get_selectedColumns(This,maxColumns,columns,nColumns) ) 

#define IAccessibleTable_get_selectedRows(This,maxRows,rows,nRows)	\
    ( (This)->lpVtbl -> get_selectedRows(This,maxRows,rows,nRows) ) 

#define IAccessibleTable_get_summary(This,accessible)	\
    ( (This)->lpVtbl -> get_summary(This,accessible) ) 

#define IAccessibleTable_get_isColumnSelected(This,column,isSelected)	\
    ( (This)->lpVtbl -> get_isColumnSelected(This,column,isSelected) ) 

#define IAccessibleTable_get_isRowSelected(This,row,isSelected)	\
    ( (This)->lpVtbl -> get_isRowSelected(This,row,isSelected) ) 

#define IAccessibleTable_get_isSelected(This,row,column,isSelected)	\
    ( (This)->lpVtbl -> get_isSelected(This,row,column,isSelected) ) 

#define IAccessibleTable_selectRow(This,row)	\
    ( (This)->lpVtbl -> selectRow(This,row) ) 

#define IAccessibleTable_selectColumn(This,column)	\
    ( (This)->lpVtbl -> selectColumn(This,column) ) 

#define IAccessibleTable_unselectRow(This,row)	\
    ( (This)->lpVtbl -> unselectRow(This,row) ) 

#define IAccessibleTable_unselectColumn(This,column)	\
    ( (This)->lpVtbl -> unselectColumn(This,column) ) 

#define IAccessibleTable_get_rowColumnExtentsAtIndex(This,index,row,column,rowExtents,columnExtents,isSelected)	\
    ( (This)->lpVtbl -> get_rowColumnExtentsAtIndex(This,index,row,column,rowExtents,columnExtents,isSelected) ) 

#define IAccessibleTable_get_modelChange(This,modelChange)	\
    ( (This)->lpVtbl -> get_modelChange(This,modelChange) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IAccessibleTable_INTERFACE_DEFINED__ */


/* Additional Prototypes for ALL interfaces */

unsigned long             __RPC_USER  BSTR_UserSize(     unsigned long *, unsigned long            , BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserMarshal(  unsigned long *, unsigned char *, BSTR * ); 
unsigned char * __RPC_USER  BSTR_UserUnmarshal(unsigned long *, unsigned char *, BSTR * ); 
void                      __RPC_USER  BSTR_UserFree(     unsigned long *, BSTR * ); 

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


