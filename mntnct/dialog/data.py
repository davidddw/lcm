import datetime
from peewee import MySQLDatabase, Model as Model
from peewee import CharField, DateTimeField, IntegerField
from peewee import DecimalField

database = MySQLDatabase('livecloud_bss', host='localhost',
                         user='root', password='security421',
                         threadlocals=True)


def string_enum(*sequential, **named):
    enums = dict(zip(sequential, sequential), **named)
    return type('StringEnum', (), enums)


class Base(Model):
    class Meta:
        database = database


class UnitPrice(Base):
    name = CharField(unique=True)
    type = CharField(index=True)
    # per-second, per-minute, per-hour, per-day, per-use
    charge_mode = CharField()
    description = CharField(default='')
    price = CharField()
    domain = CharField()

    class Meta:
        db_table = 'unit_price'


class PricingPlan(Base):
    name = CharField(unique=True)
    type = CharField(index=True)
    description = CharField(default='')
    content = CharField()
    coupon = CharField()
    domain = CharField()
    # never/after_this_date/in_x_days
    expiry_type = CharField(default='never')
    expires_in = IntegerField(null=True)
    expires_after = DateTimeField(null=True)

    class Meta:
        db_table = 'pricing_plan'


class ProductSpec(Base):
    lcuuid = CharField(unique=True)
    name = CharField(default='')
    type = IntegerField()
    charge_mode = IntegerField()
    state = IntegerField()
    price = DecimalField(max_digits=18, decimal_places=9)
    content = CharField()
    plan_name = CharField()
    product_type = IntegerField()
    domain = CharField()

    class Meta:
        db_table = 'product_specification'


class Domain(Base):
    lcuuid = CharField(unique=True)
    name = CharField(default='')

    class Meta:
        db_table = 'domain'


class PromotionRules(Base):
    name = CharField(default='')
    domain = CharField()
    purchase_type = CharField()
    purchase_quantity = IntegerField()
    present_type = CharField()
    present_quantity = IntegerField()
    present_days = IntegerField()
    event_start_time = DateTimeField(default=datetime.datetime.now)
    event_end_time = DateTimeField(null=True)
    description = CharField(default='')

    class Meta:
        db_table = 'promotion_rules'
