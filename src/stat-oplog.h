
struct oplog_t{
    int32_t op;
    int32_t src_role;
    int32_t src_role_id;
    int32_t dst_role;
    int32_t dst_role_id;
    int32_t business_id;
    int32_t timestamp;
};
