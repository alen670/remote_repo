import pandas as pd

# 读入文件
data=pd.read_excel('./实战/test.xlsx')
# 数据划分
data['year']=data['type'].apply(lambda x:x.split('/')[0].strip())
data['c']=data['type'].apply(lambda x:x.split('/')[1].strip())
data['t']=data['type'].apply(lambda x:x.split('/')[2].strip())
writer=pd.ExcelFile('temp.xlsx')
for i in data['year'].unique():
    data[data['year']==i].to_excel(writer,sheet_name=i)
writer.close()
# data.to_excel(writer,sheet_name='原始数据')

# 生成器
# type_list=set(z for i in data['t'] for z in i.split(''))
# for ty in type_list:
#     data[data['t'].str.contains(ty)].to_excel(writer,sheet_name=ty)