## Bug with not instantiated lazy dirs

### Before instantiation

```
  {"entities":[{"inputs":["/cells/"],"name":"/sk/mappers/map/ext_5/","reads":[],"type":{"constructors":[{"name":"CallCompute","parameters":[{"type":"collection","value":"/sk/mappers/lazy/ext_4/"}]}],"operator":"map"}}],"inputs":[{"alias":"cells","name":"/cells/"}],"outputs":[{"alias":"output","name":"/sk/mappers/map/ext_5/"}],"reads":[{"collection":"/sk/mappers/lazy/ext_4/","key":"#tag"}]}
```

### After instantiation

```  
  {"entities":[{"inputs":["/cells/"],"name":"/sk/mappers/map/ext_5/","reads":["/sk/mappers/lazy/ext_4/"],"type":{"constructors":[{"name":"CallCompute","parameters":[{"type":"collection","value":"/sk/mappers/lazy/ext_4/"}]}],"operator":"map"}},{"inputs":[],"name":"/sk/mappers/lazy/ext_4/","reads":["/cells/","/sk/mappers/lazy/ext_4/"],"type":{"constructors":[{"name":"ComputeExpression","parameters":[{"type":"collection","value":"/cells/"}]}],"operator":"lazy"}}],"inputs":[{"alias":"cells","name":"/cells/"}],"outputs":[{"alias":"output","name":"/sk/mappers/map/ext_5/"}],"reads":[{"collection":"/sk/mappers/lazy/ext_4/","key":"#tag"}]}
```

