namespace cpp common

exception OperationFailed {
  1: i32 code,
  2: string reason
}

service CommonService {
  void ping()
}