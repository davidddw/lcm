import sys


class Command(dict):
    """Basic command class"""

    class _Cmd(object):
        def __init__(self, name, rqd_params, opt_params, func):
            self.name = name
            self.rqd_params = rqd_params
            self.opt_params = opt_params
            self.call = func

    # decorator
    def define(self, name, rqd_params='', opt_params='', description='',
               example='', hidden=False):
        def decorator(f):
            f.__doc__ = Command._generate_doc(name, rqd_params, opt_params,
                                              description, example)
            self[name] = Command._Cmd(name, rqd_params, opt_params, f)
            return f
        return decorator

    def help(self, op):
        return self[op].call.__doc__

    def call(self, op, args):
        assert isinstance(args, dict)
        if op not in self:
            return -1
        if self[op].rqd_params.strip() == '':
            params1 = []
        else:
            params1 = self[op].rqd_params.strip().split(', ')
        for p in params1:
            if p not in args:
                print >>sys.stderr, 'Error: No %s specified' % p
                sys.exit(-1)
        params2 = self[op].opt_params.strip().split(', ')
        all_params = params1 + params2
        for p in args.keys():
            if p not in all_params:
                print >>sys.stderr, 'Error: Unknown parameter %s' % p
                sys.exit(-1)
        return self[op].call(args)

    @classmethod
    def _generate_doc(cls, name, rqd_params, opt_params, desc, example):
        return ("command name            : {0}\n"
                "        reqd params     : {1}\n"
                "        optional params : {2}\n"
                "        description     : {3}\n"
                "        example         : {4}\n"
                ).format(name, rqd_params, opt_params, desc, example)

    def add_commands(self, other):
        assert isinstance(other, Command)
        self.update(other)
